/*
 * Campagnol main code
 *
 * Copyright (C) 2007 Antoine Vianey
 *               2008-2011 Florent Bondoux
 *
 * This file is part of Campagnol.
 *
 * Campagnol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Campagnol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Campagnol.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "campagnol.h"

#include <time.h>
#include <pthread.h>

#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>

#include "communication.h"
#include "../common/pthread_wrap.h"
#include "net_socket.h"
#include "peer.h"
#include "dtls_utils.h"
#include "tun_device.h"
#include "../common/log.h"
#include "../common/bss_fifo.h"

struct tb_state global_rate_limiter;


/* Initialise a message with the given fields */
static inline void init_smsg(message_t *smsg, unsigned char type, uint32_t ip1, uint32_t ip2) {
    memset(smsg, 0, sizeof(message_t));
    smsg->type = type;
    smsg->ip1.s_addr = ip1;
    smsg->ip2.s_addr = ip2;
}

/* Initialise a timeval structure to use with select */
static inline void init_timeout(struct timeval *timeout) {
    timeout->tv_sec = SELECT_DELAY_SEC;
    timeout->tv_usec = SELECT_DELAY_USEC;
}

/*
 * Handler for SIGALRM
 * send a PING message
 */
static int sockfd_global;
void handler_sigTimerPing(int sig __attribute__((unused))) {
    message_t smsg;
    /* send a PING message to the RDV server */
    init_smsg(&smsg, PING,0,0);
    ssize_t s = xsendto(sockfd_global, &smsg, sizeof(smsg), 0,
            (struct sockaddr *) &config.serverAddr, sizeof(config.serverAddr));
    if (s == -1) log_error(errno, "PING");
}

/*
 * Compute the internet checksum
 * See RFC 1071
 * http://tools.ietf.org/html/rfc1071
 * This function is based on the example in the RFC
 */
static uint16_t compute_csum(uint16_t *addr, size_t count){
   register int32_t sum = 0;

    while( count > 1 )  {
        /*  This is the inner loop */
        sum += * addr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if( count > 0 )
        sum += (* (unsigned char *) addr) << 8;

    /*  Fold 32-bit sum to 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);

    return (uint16_t) ~sum;
}


/*
 * Function sending the punch messages for UDP hole punching
 * arg: struct punch_arg
 */
static void *punch(void *arg) {
    int i;
    struct client *peer = (struct client *)arg;
    message_t smsg;
    init_smsg(&smsg, PUNCH, config.vpnIP.s_addr, 0);
    log_message_level(2, "Punching %s %d", inet_ntoa(peer->clientaddr.sin_addr), ntohs(peer->clientaddr.sin_port));
    for (i=0; i<PUNCH_NUMBER; i++) {
        CLIENT_MUTEXLOCK(peer);
        if (peer->state == CLOSED) {
            CLIENT_MUTEXUNLOCK(peer);
            break;
        }
        CLIENT_MUTEXUNLOCK(peer);
        xsendto(peer->sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&(peer->clientaddr), sizeof(peer->clientaddr));
        usleep(PUNCH_DELAY_USEC);
    }
    peers_decr_ref(peer, 1);
    pthread_exit(NULL);
}

/*
 * Create a thread executing "punch"
 */
static void start_punch(struct client *peer) {
    peers_incr_ref(peer);
    createDetachedThread(punch, (void *)peer);
}

/*
 * Thread associated with each SSL stream
 *
 * read the outgoing packets from peer->out_fifo
 * and write them to the SSL stream
 */
static void *SSL_writing(void *args) {
    fd_set set;
    struct client *peer = (struct client *) args;
    int r, w, err;
    int packet_len = MESSAGE_MAX_LENGTH;
    char *packet = CHECK_ALLOC_FATAL(malloc(packet_len));

    /* stop dropping packets when this fifo is full */
    BIO_ctrl(peer->out_fifo, BIO_CTRL_FIFO_SET_DROPTAIL, 0, NULL);

    while (1) {
        r = BIO_read(peer->out_fifo, packet, packet_len);
        if (r == 0)
            break;
        do {
            w = SSL_write(peer->ssl, packet, r);
            if (w <= 0) {
                if (BIO_should_write(peer->wbio)) {
                    FD_ZERO(&set);
                    FD_SET(peer->sockfd, &set);
                    select(peer->sockfd+1, NULL, &set, NULL, NULL);
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        } while (1);

        if (w <= 0) {
            err = SSL_get_error(peer->ssl, w);
            if (err == SSL_ERROR_ZERO_RETURN)
                break;
            else if (err == SSL_ERROR_SSL && !SSL_get_shutdown(peer->ssl)) {
                ERR_print_errors_fp(stderr);
                break;
            }
        }
    }
    free(packet);
    SSL_REMOVE_ERROR_STATE;
    peers_decr_ref(peer, 1);
    return NULL;
}

/*
 * Create the SSL_writing thread.
 * Must be called when the DTLS session is opened
 */
static void start_SSL_writing(struct client *peer) {
    peers_incr_ref(peer);
    createDetachedThread(SSL_writing, peer);
}

/*
 * Kill the SSL_writing thread
 */
static void end_SSL_writing(struct client *peer) {
    BIO_write(peer->out_fifo, &peer, 0);
}

#if 0
#   define CHANGE_STATE(peer,_state) \
    printf("%X %d->%d\n", peer->vpnIP.s_addr, peer->state, _state); \
    peer->state = _state;
#else
#   define CHANGE_STATE(peer,_state) peer->state = _state;
#endif

/*
 * This thread does most of the work to handle a connection
 * - hole punching,
 * - DTLS session opening and closing
 * - starting/ending the SSL_writing thread
 * - handling the SSL reading
 *
 * The SSL stream is fed by calling BIO_write with peer->rbio
 *
 * args: the connected peer (struct client *)
 *
 * TODO: this design with two threads per session is not supported by OpenSSL
 * and broken. It breaks at least SSL_get_error (WANT_READ and WANT_WRITE are
 * not reliable) and it would also break SSL renegotiations.
 */
static void * peer_handling(void * args) {
    int r;
    int err;
    packet_t u; // union used to receive the messages
    message_t smsg;
    struct client *peer = (struct client*) args;
#ifndef HAVE_CYGWIN
    int tunfd = peer->tunfd;
#endif
    struct timespec timeout_connect;            // timeout
    time_t timestamp, last_time = 0;

    int u_len = 1<<16;
    u.raw = CHECK_ALLOC_FATAL(malloc(u_len));


    while (1) {
        CLIENT_MUTEXLOCK(peer);
        if (peer->state == NEW) {
            if (end_campagnol) {
                CHANGE_STATE(peer, CLOSED);
                CLIENT_MUTEXUNLOCK(peer);
                continue;
            }

            /* ask the RDV server for a new connection with peer */
            init_smsg(&smsg, ASK_CONNECTION, peer->vpnIP.s_addr, 0);
            xsendto(peer->sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));

            /* wait for and check answer */
            clock_gettime(CLOCK_REALTIME, &timeout_connect);
            timeout_connect.tv_sec += 3; // wait 3 secs
            r = conditionTimedwait(&peer->cond_connected, &peer->mutex, &timeout_connect);
            if (r != 0 || (r == 0 && peer->rdv_answer == REJ_CONNECTION) || end_campagnol) {
                // timeout or connection rejected by the RDV
                CHANGE_STATE(peer, CLOSED);
            }
            else {
                // the RDV accepted the connection,
                // we now know the public endpoint of the peer
                if (peers_register_endpoint(peer) != 0) {
                    CHANGE_STATE(peer, CLOSED);
                }
                else {
                    CHANGE_STATE(peer, PUNCHING);
                }
            }
            CLIENT_MUTEXUNLOCK(peer);
        }
        else if (peer->state == PUNCHING) {
            if (end_campagnol) {
                CHANGE_STATE(peer, CLOSED);
                CLIENT_MUTEXUNLOCK(peer);
                continue;
            }

            start_punch(peer);
            clock_gettime(CLOCK_REALTIME, &timeout_connect);
            timeout_connect.tv_sec += 3; // wait 3 secs
            if (conditionTimedwait(&peer->cond_connected, &peer->mutex, &timeout_connect) != 0) {
                // timeout
                CHANGE_STATE(peer, CLOSED);
                CLIENT_MUTEXUNLOCK(peer);
                init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                xsendto(peer->sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
            }
            else if (end_campagnol) {
                CHANGE_STATE(peer, CLOSED);
                CLIENT_MUTEXUNLOCK(peer);
            }
            else {
                CHANGE_STATE(peer, LINKED);
                CLIENT_MUTEXUNLOCK(peer);
            }
        }
        else if (peer->state == LINKED) {
            if (end_campagnol) {
                CHANGE_STATE(peer, CLOSED);
                CLIENT_MUTEXUNLOCK(peer);
                continue;
            }
            CLIENT_MUTEXUNLOCK(peer);
            /* DTLS handshake */
            BIO_ctrl(peer->wbio, BIO_CTRL_DGRAM_SET_PEER, 0, &peer->clientaddr);
            while (1) {
                r = SSL_do_handshake(peer->ssl);
                if (r == 1)
                    break;
                else {
                    err = SSL_get_error(peer->ssl, r);
                    if (err == SSL_ERROR_WANT_READ) {
                        DTLSv1_handle_timeout(peer->ssl);
                    }
                    else
                        break;
                }
            }

            if (r != 1) {
                log_message("Error during DTLS handshake with peer %s", inet_ntoa(peer->vpnIP));
                ERR_print_errors_fp(stderr);
                init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                xsendto(peer->sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                CLIENT_MUTEXLOCK(peer);
                CHANGE_STATE(peer, CLOSED);
                CLIENT_MUTEXUNLOCK(peer);
            }
            else {
                unsigned int mac_size, blocksize;
                unsigned int pkt_size;
                CLIENT_MUTEXLOCK(peer);
                CHANGE_STATE(peer, ESTABLISHED);
                peers_update_peer_time(peer, time(NULL));

                // Get the mac size and the block size, compute the required MTU
                // for OpenSSL.
                if (peer->ssl->write_hash)
#if OPENSSL_VERSION_NUMBER >= 0x10000000
                    mac_size = EVP_MD_CTX_size(peer->ssl->write_hash);
#else
                    mac_size = EVP_MD_size(peer->ssl->write_hash);
#endif
                else
                    mac_size = 0;

                if (peer->ssl->enc_write_ctx &&
                    (EVP_CIPHER_mode( peer->ssl->enc_write_ctx->cipher) & EVP_CIPH_CBC_MODE))
                    blocksize = EVP_CIPHER_block_size(peer->ssl->enc_write_ctx->cipher);
                else
                    blocksize = 0;

                pkt_size = config.tun_mtu + 1 + blocksize + mac_size; // padding field + IV + MAC
                peer->ssl->d1->mtu = pkt_size;
                if (pkt_size & (blocksize - 1)) // not a multiple of the block size, need to add padding
                    peer->ssl->d1->mtu += blocksize - (pkt_size & (blocksize - 1));

                // DTLS header
                peer->ssl->d1->mtu += DTLS1_RT_HEADER_LENGTH; // Header length

                log_message_level(2, "Internal MTU adjusted to %u", peer->ssl->d1->mtu);

                CLIENT_MUTEXUNLOCK(peer);
            }
        }
        else if (peer->state == ESTABLISHED) {
            CLIENT_MUTEXUNLOCK(peer);
            int end_reading_loop = 0;
            log_message_level(1, "New DTLS connection opened with peer %s", inet_ntoa(peer->vpnIP));

            /* Start the writing thread */
            start_SSL_writing(peer);

            /* And do the reading stuff */
            while (!end_reading_loop) {
                /* Read and uncrypt a message, send it on the TUN device */
                r = SSL_read(peer->ssl, u.raw, u_len);
                timestamp = time(NULL);
                if (BIO_should_read(peer->rbio)) { // timeout on SSL_read
                    // check whether the connection is active and send keepalive messages
                    if (timestamp != last_time) {
                        CLIENT_MUTEXLOCK(peer);
                        if (timestamp - peer->last_keepalive > (time_t) config.keepalive) {
                            init_smsg(&smsg, PUNCH_KEEP_ALIVE, 0, 0);
                            xsendto(peer->sockfd ,&smsg, sizeof(smsg), 0, (struct sockaddr *)&(peer->clientaddr), sizeof(peer->clientaddr));
                            peer->last_keepalive = timestamp;
                        }

                        if ((!peer->is_dtls_client && (timestamp - peer->time) > (config.timeout + 10))
                                || (peer->is_dtls_client && (timestamp - peer->time) > config.timeout)) {
                            log_message_level(2, "Session timeout: %s", inet_ntoa(peer->vpnIP));
                            log_message_level(1, "Closing DTLS connection with peer %s", inet_ntoa(peer->vpnIP));
                            init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                            xsendto(peer->sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                            CHANGE_STATE(peer, CLOSED);
                            CLIENT_MUTEXUNLOCK(peer);
                            r = SSL_shutdown(peer->ssl);
                            while (r == 0 && peer->ssl->s3->alert_dispatch) {
                                /* data are still being written out,
                                 * wait and retry */
                                usleep(10000);
                                /* clear some flags before retrying */
                                peer->ssl->s3->alert_dispatch = 0;
                                peer->ssl->shutdown&=~SSL_SENT_SHUTDOWN;
                                SSL_shutdown(peer->ssl);
                            }
                            ERR_print_errors_fp(stderr);
                            end_SSL_writing(peer);
                            end_reading_loop = 1;
                        }
                        else {
                            CLIENT_MUTEXUNLOCK(peer);
                        }
                        last_time = timestamp;
                    }
                    continue;
                }
                if (r <= 0) { // error or shutdown
                    err = SSL_get_error(peer->ssl, r);
                    switch(err) {
                        case SSL_ERROR_WANT_WRITE:
                        case SSL_ERROR_WANT_READ:
                            continue;
                            break;
                        case SSL_ERROR_ZERO_RETURN: // shutdown received
                            log_message_level(1, "DTLS connection closed by peer %s", inet_ntoa(peer->vpnIP));
                            break;
                        case SSL_ERROR_SYSCALL: // end_peer_handling or error
                            if (peer->shutdown) { //end_peer_handling was called
                                log_message_level(1, "Closing DTLS connection with peer %s", inet_ntoa(peer->vpnIP));
                                r = SSL_shutdown(peer->ssl);
                                while (r == 0 && peer->ssl->s3->alert_dispatch) {
                                    /* data are still being written out,
                                     * wait and retry */
                                    usleep(10000);
                                    /* clear some flags before retrying */
                                    peer->ssl->s3->alert_dispatch = 0;
                                    peer->ssl->shutdown&=~SSL_SENT_SHUTDOWN;
                                    SSL_shutdown(peer->ssl);
                                }
                                ERR_print_errors_fp(stderr);
                            }
                            else {// I don't think we can get there
                                ERR_print_errors_fp(stderr);
                                log_message("DTLS error, shutting down the connexion.");
                            }
                            break;
                        default:
                            ERR_print_errors_fp(stderr);
                            log_message("DTLS error, shutting down the connexion.");
                            break;
                    }
                    CLIENT_MUTEXLOCK(peer);
                    CHANGE_STATE(peer, CLOSED);
                    CLIENT_MUTEXUNLOCK(peer);
                    end_SSL_writing(peer);
                    end_reading_loop = 1;
                }
                else {// everything's fine
                    CLIENT_MUTEXLOCK(peer);
                    peers_update_peer_time(peer, timestamp);
                    CLIENT_MUTEXUNLOCK(peer);
                    if (config.debug)
                        printf(
                                "<< Received a VPN message: size %d from SRC = %"PRIu32".%"PRIu32".%"PRIu32".%"PRIu32" to DST = %"PRIu32".%"PRIu32".%"PRIu32".%"PRIu32"\n",
                                r, (ntohl(u.ip->ip_src.s_addr) >> 24) & 0xFF,
                                (ntohl(u.ip->ip_src.s_addr) >> 16) & 0xFF,
                                (ntohl(u.ip->ip_src.s_addr) >> 8) & 0xFF,
                                (ntohl(u.ip->ip_src.s_addr) >> 0) & 0xFF,
                                (ntohl(u.ip->ip_dst.s_addr) >> 24) & 0xFF,
                                (ntohl(u.ip->ip_dst.s_addr) >> 16) & 0xFF,
                                (ntohl(u.ip->ip_dst.s_addr) >> 8) & 0xFF,
                                (ntohl(u.ip->ip_dst.s_addr) >> 0) & 0xFF);
                    /*
                     * If dest IP = VPN broadcast VPN
                     * The TUN device creates a point to point connection which
                     * does not transmit the broadcast IP
                     * So alter the dest. IP to the normal VPN IP
                     * and compute the new checksum
                     */
                    if (u.ip->ip_dst.s_addr == config.vpnBroadcastIP.s_addr) {
                        u.ip->ip_dst.s_addr = config.vpnIP.s_addr;
                        u.ip->ip_sum = 0; // the checksum field is set to 0 for the calculation
                        u.ip->ip_sum = compute_csum((uint16_t*) u.ip, sizeof(*u.ip));
                    }
                    // send it to the TUN device
#ifdef HAVE_CYGWIN
                    write_tun(u.raw, r);
#else
                    write_tun(tunfd, u.raw, r);
#endif
                }
            }

        }
        else if (peer->state == CLOSED) {
            CLIENT_MUTEXUNLOCK(peer);
            /* remove one ref. for this thread and the last ref to destroy the
             * client
             */
            peers_decr_ref(peer, 2); // now this peer is dead.
            break;
        }
    }


    SSL_REMOVE_ERROR_STATE;
    free(u.raw);
    return NULL;
}


/*
 * Create the peer_handling thread
 */
static void start_peer_handling(struct client *peer) {
    peers_incr_ref(peer);
    createDetachedThread(peer_handling, peer);
}

/*
 * Stop the peer_handling thread.
 * The peer is detroyed after calling peer_handling.
 *
 * islocked: the peer's mutex is currently locked. It will be unlocked when the
 * function returns.
 */
static inline void end_peer_handling(struct client *peer, int islocked) {
    if (!islocked) {
        islocked = 1;
        CLIENT_MUTEXLOCK(peer);
    }
    peer->shutdown = 1;
    /* SSL_read will return with 0, which is the same
     * as when we receive a DTLS shutdown alert
     * Just pass a non NULL pointer to BIO_write
     */
    if (islocked)
        CLIENT_MUTEXUNLOCK(peer);
    BIO_write(peer->rbio, &peer, 0);
}

/*
 * Perform the registration of the client to the RDV server
 * args: rdv_args structure with the FIFO and the socket descriptor
 */
static int register_rdv(struct rdv_args *args) {
    message_t smsg, rmsg;

    int r;
    int registered = 0;
    int registeringTries = 0;

    /* Create a HELLO message */
    log_message_level(1, "Registering with the RDV server...");
    if (config.send_local_addr == 1) {
        init_smsg(&smsg, HELLO, config.vpnIP.s_addr, config.localIP.s_addr);
        smsg.port = htons(config.localport);
    }
    else if (config.send_local_addr == 2) {
        init_smsg(&smsg, HELLO, config.vpnIP.s_addr,
                config.override_local_addr.sin_addr.s_addr);
        smsg.port = config.override_local_addr.sin_port;
    }
    else {
        init_smsg(&smsg, HELLO, config.vpnIP.s_addr, 0);
    }

    while (!registered && registeringTries < MAX_REGISTERING_TRIES
            && !end_campagnol) {
        /* sending HELLO to the server */
        registeringTries++;
        log_message_level(2, "Sending HELLO");
        if (xsendto(args->sockfd, &smsg, sizeof(smsg), 0,
                (struct sockaddr *) &config.serverAddr,
                sizeof(config.serverAddr)) == -1) {
            log_error(errno, "sendto");
        }

        r = BIO_read(args->fifo, &rmsg, sizeof(rmsg));

        if (r == sizeof(rmsg)) {
            switch (rmsg.type) {
                case OK:
                    /* Registration OK */
                    log_message_level(1, "Registration complete");
                    registered = 1;
                    break;
                case NOK:
                    /* The RDV server rejected the client */
                    log_message("The RDV server rejected the client");
                    sleep(1);
                    break;
                default:
                    log_message("The RDV server replied with something strange... (message code is %d)", rmsg.type);
                    break;
            }
        }
    }

    /* Registration failed. The server may be down */
    if (!registered) {
        log_message_level(1, "The connection with the RDV server failed");
    }

    return registered;
}

/*
 * This thread handle the connection with the RDV server. It doesn't do the
 * initial registration.
 *
 * arg: (struct rdv_args *)
 */
static void *rdv_handling(void *arg) {
    struct rdv_args *args = (struct rdv_args *) arg;
    message_t rmsg;
    struct client *peer;
    int r;

    while (!end_campagnol) {
        r = BIO_read(args->fifo, &rmsg, sizeof(message_t));
        if (r > 0) {
            /* which type */
            switch (rmsg.type) {
                case REJ_CONNECTION:
                    peer = peers_get_by_VPN(&rmsg.ip1);
                    if (peer != NULL) {
                        peer->rdv_answer = REJ_CONNECTION;
                        CLIENT_MUTEXUNLOCK(peer);
                        conditionSignal(&peer->cond_connected);
                        peers_decr_ref(peer, 1);
                    }
                    break;
                case ANS_CONNECTION:
                    peer = peers_get_by_VPN(&rmsg.ip2);
                    if (peer != NULL) {
                        peer->rdv_answer = ANS_CONNECTION;
                        peer->clientaddr.sin_addr = rmsg.ip1;
                        peer->clientaddr.sin_port = rmsg.port;
                        CLIENT_MUTEXUNLOCK(peer);
                        conditionSignal(&peer->cond_connected);
                        peers_decr_ref(peer, 1);
                    }
                    break;
                case FWD_CONNECTION:
                    peer = peers_get_by_VPN(&rmsg.ip2);
                    if (peer == NULL) {
                        /* Unknown client, add a new structure */
                        peer = peers_add_caller(args->sockfd, args->tunfd,
                                PUNCHING, time(NULL), rmsg.ip1, rmsg.port,
                                rmsg.ip2);
                        if (peer == NULL) {
                            /* max number of clients */
                            break;
                        }
                        /* start punching */
                        start_peer_handling(peer);
                        peers_decr_ref(peer, 1);
                    }
                    else {
                        CLIENT_MUTEXUNLOCK(peer);
                        peers_decr_ref(peer, 1);
                    }
                    break;
                case RECONNECT:
                    register_rdv(args);
                    break;
                case PONG:
                    break;
                default:
                    break;
            }
        }
    }

    return NULL;
}

/*
 * Manage the incoming messages from the UDP socket
 * argument: struct comm_args *
 */
static void * comm_socket(void * argument) {
    struct comm_args * args = argument;
    int sockfd = args->sockfd;

    int r;
    int r_select;
    fd_set fd_select;                           // for the select call
    struct timeval timeout;                     // timeout used with select
    packet_t u;
    struct sockaddr_in unknownaddr;             // address of the sender
    socklen_t len = sizeof(struct sockaddr_in);
    struct client *peer;

    size_t u_len = 1<<16;
    u.raw = CHECK_ALLOC_FATAL(malloc(u_len));

    while (!end_campagnol) {
        /* select call initialisation */
        FD_ZERO(&fd_select);
        FD_SET(sockfd, &fd_select);

        init_timeout(&timeout);
        r_select = select(sockfd+1, &fd_select, NULL, NULL, &timeout);

        /* MESSAGE READ FROM THE SOCKET */
        if (r_select > 0) {
            while ((r = (int) recvfrom(sockfd,u.raw,u_len,0,(struct sockaddr *)&unknownaddr,&len)) != -1) {
                /* from the RDV server ? */
                if (config.serverAddr.sin_addr.s_addr == unknownaddr.sin_addr.s_addr
                    && config.serverAddr.sin_port == unknownaddr.sin_port) {
                    if (r == sizeof(message_t))
                        BIO_write(args->rdvargs->fifo, u.raw, r);
                }
                /* Message from another peer */
                else {
                    if (config.debug) printf("<  Received a UDP packet: size %d from %s:%d\n", r, inet_ntoa(unknownaddr.sin_addr), ntohs(unknownaddr.sin_port));
                    if (r >= (int) sizeof(dtlsheader_t) &&
                            (u.dtlsheader->contentType == DTLS_APPLICATION_DATA
                            || u.dtlsheader->contentType == DTLS_HANDSHAKE
                            || u.dtlsheader->contentType == DTLS_ALERT
                            || u.dtlsheader->contentType == DTLS_CHANGE_CIPHER_SPEC)) {
                        /* It's a DTLS packet, send it to the associated peer_handling thread using the FIFO BIO */
                        peer = peers_get_by_endpoint(&unknownaddr);
                        if (peer != NULL) {
                            if (peer->state == ESTABLISHED || peer->state == LINKED) {
                                CLIENT_MUTEXUNLOCK(peer);
                                BIO_write(peer->rbio, u.raw, r);
                            }
                            else {
                                CLIENT_MUTEXUNLOCK(peer);
                            }
                            peers_decr_ref(peer, 1);
                        }
                        else if (u.dtlsheader->contentType == DTLS_APPLICATION_DATA) {
                            /* We received a DTLS record from an unknown peer.
                             * This may be due to a lost close notification, or we
                             * died uncleanly and were restarted.
                             * Let's reply with a fatal alert record...
                             */
                            unsigned char alert_mess[15];
                            dtlsheader_t *alert_mess_hdr = (dtlsheader_t *)alert_mess;
                            alert_mess_hdr->contentType = DTLS_ALERT;
                            alert_mess_hdr->version = u.dtlsheader->version;
                            alert_mess_hdr->epoch = u.dtlsheader->epoch;
                            alert_mess_hdr->seq_number = u.dtlsheader->seq_number;
                            alert_mess[11] = 0; // length
                            alert_mess[12] = 2; // length
                            alert_mess[13] = 2; // fatal
                            alert_mess[14] = 80; // internal error
                            xsendto(sockfd, alert_mess, 15, 0, (struct sockaddr *)&unknownaddr, len);
                        }
                    }
                    else if (r == sizeof(message_t)) {
                        switch (u.message->type) {
                            /* UDP hole punching */
                            case PUNCH :
                                /* we can now reach the client */
                                peer = peers_get_by_endpoint(&unknownaddr);
                                if (peer != NULL) {
                                    conditionSignal(&peer->cond_connected);
                                    CLIENT_MUTEXUNLOCK(peer);
                                    peers_decr_ref(peer, 1);
                                }
                                break;
                            case PUNCH_KEEP_ALIVE:
                                /* receive a keepalive message */
                            default :
                                break;
                        }
                    }
                }
            }
        }
    }

    free(u.raw);
    SSL_REMOVE_ERROR_STATE;
    return NULL;
}


/*
 * Manage the incoming messages from the TUN device
 * argument: struct comm_args *
 */
static void * comm_tun(void * argument) {
    struct comm_args * args = argument;
    int sockfd = args->sockfd;
    int tunfd = args->tunfd;

    int r;
    int r_select;
#ifndef HAVE_CYGWIN
    fd_set fd_select;                           // for the select call
    struct timeval timeout;                     // timeout used with select
#endif
    packet_t u;
    struct in_addr peer_addr;
    struct client *peer;

    u.raw = CHECK_ALLOC_FATAL(malloc(MESSAGE_MAX_LENGTH));

    while (!end_campagnol) {
#ifdef HAVE_CYGWIN
        r_select = read_tun_wait(u.raw, MESSAGE_MAX_LENGTH, SELECT_DELAY_SEC*1000 + SELECT_DELAY_USEC/1000);
#else
        /* select call initialisation */
        init_timeout(&timeout);
        FD_ZERO(&fd_select);
        FD_SET(tunfd, &fd_select);
        r_select = select(tunfd+1, &fd_select, NULL, NULL, &timeout);
#endif


        /* MESSAGE READ FROM TUN DEVICE */
        if (r_select > 0) {
#ifdef HAVE_CYGWIN
            r = read_tun_finalize();
#else
            r = (int) read_tun(tunfd, u.raw, MESSAGE_MAX_LENGTH);
#endif
            if (config.debug)
                printf(
                        ">> Sending a VPN message: size %d from SRC = %"PRIu32".%"PRIu32".%"PRIu32".%"PRIu32" to DST = %"PRIu32".%"PRIu32".%"PRIu32".%"PRIu32"\n",
                        r, (ntohl(u.ip->ip_src.s_addr) >> 24) & 0xFF,
                        (ntohl(u.ip->ip_src.s_addr) >> 16) & 0xFF,
                        (ntohl(u.ip->ip_src.s_addr) >> 8) & 0xFF,
                        (ntohl(u.ip->ip_src.s_addr) >> 0) & 0xFF,
                        (ntohl(u.ip->ip_dst.s_addr) >> 24) & 0xFF,
                        (ntohl(u.ip->ip_dst.s_addr) >> 16) & 0xFF,
                        (ntohl(u.ip->ip_dst.s_addr) >> 8) & 0xFF,
                        (ntohl(u.ip->ip_dst.s_addr) >> 0) & 0xFF);

            peer_addr.s_addr = u.ip->ip_dst.s_addr;

            /*
             * If dest IP = VPN broadcast VPN
             */
            if (peer_addr.s_addr == config.vpnBroadcastIP.s_addr) {
                GLOBAL_MUTEXLOCK;
                peer = peers_list;
                while (peer != NULL) {
                    struct client *next = peer->next;
                    CLIENT_MUTEXLOCK(peer);
                    if (peer->state == ESTABLISHED) {
                        BIO_write(peer->out_fifo, u.raw, r);
                    }
                    CLIENT_MUTEXUNLOCK(peer);
                    peer = next;
                }
                GLOBAL_MUTEXUNLOCK;
            }
            /*
             * Local packet, loop back
             */
            else if (peer_addr.s_addr == config.vpnIP.s_addr) {
#ifdef HAVE_CYGWIN
                write_tun(u.raw, r);
#else
                write_tun(tunfd, u.raw, r);
#endif
            }
            else {
                peer = peers_get_by_VPN(&peer_addr);
                if (peer == NULL) {
                    peer = peers_add_requested(sockfd, tunfd, NEW, time(NULL), peer_addr);
                    if (peer == NULL) {
                        continue;
                    }
                    BIO_write(peer->out_fifo, u.raw, r);
                    start_peer_handling(peer);
                }
                else {
                    if (peer->state != CLOSED) {
                        peers_update_peer_time(peer,time(NULL));
                        CLIENT_MUTEXUNLOCK(peer);
                        BIO_write(peer->out_fifo, u.raw, r);
                    }
                    else {
                        CLIENT_MUTEXUNLOCK(peer);
                    }
                }
                peers_decr_ref(peer, 1);
            }
        }
    }

#ifdef HAVE_CYGWIN
    read_tun_cancel();
#endif

    free(u.raw);
    SSL_REMOVE_ERROR_STATE;
    return NULL;
}


/*
 * Start the VPN:
 * start a thread running comm_tun and another one running comm_socket
 *
 * set end_campagnol to 1 un order to stop both threads (and others)
 */
int start_vpn(int sockfd, int tunfd) {
    message_t smsg;
    struct comm_args args;
    struct rdv_args rdvargs;
    int registered;
    pthread_t th_socket, th_rdv;
    struct itimerval timer_ping;
    struct timeval timeout;

    args.sockfd = sockfd;
    args.tunfd = tunfd;
    rdvargs.sockfd = sockfd;
    rdvargs.tunfd = tunfd;
    rdvargs.fifo = BIO_new_fifo(10, sizeof(message_t));
    timeout.tv_sec = SELECT_DELAY_SEC;
    timeout.tv_usec = SELECT_DELAY_USEC;
    BIO_ctrl(rdvargs.fifo, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
    args.rdvargs = &rdvargs;

    /* initialize the global rate limiter */
    if (config.tb_client_size != 0) {
        tb_init(&global_rate_limiter, config.tb_client_size, (double) config.tb_client_rate, 8, 1);
    }

    peers_mutex_init();
    if (initDTLS() == -1) {
        return -1;
    }

    th_socket = createThread(comm_socket, &args);

    registered = register_rdv(&rdvargs);

    if (registered) {
        /* set a timer for the ping messages */
        timer_ping.it_interval.tv_sec = TIMER_PING_SEC;
        timer_ping.it_interval.tv_usec = TIMER_PING_USEC;
        timer_ping.it_value.tv_sec = TIMER_PING_SEC;
        timer_ping.it_value.tv_usec = TIMER_PING_USEC;
        sockfd_global = sockfd;
        setitimer(ITIMER_REAL, &timer_ping, NULL);

        /* start the RDV handler and do some work */
        th_rdv = createThread(rdv_handling, &rdvargs);
        comm_tun(&args);

        joinThread(th_rdv, NULL);
    }
    else {
        end_campagnol = 1;
    }

    joinThread(th_socket, NULL);

    BIO_free(rdvargs.fifo);

    GLOBAL_MUTEXLOCK;
    struct client *peer, *next;
    peer = peers_list;
    while (peer != NULL) {
        next = peer->next;
        CLIENT_MUTEXLOCK(peer);
        end_peer_handling(peer, 1); // this also unlock the mutex
        // don't wait for the peer_handling thread terminaison here !
        // the thread needs to lock the mutex
        peer = next;
    }
    GLOBAL_MUTEXUNLOCK;

    init_smsg(&smsg, BYE, config.vpnIP.s_addr, 0);
    log_message_level(2, "Sending BYE");
    if (xsendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr)) == -1) {
        log_error(errno, "sendto");
    }

    // wait for all the peer_handling threads to finish
    while (peers_n_clients != 0) { usleep(100000); }

    if (config.tb_client_size != 0) {
        tb_clean(&global_rate_limiter);
    }

    clearDTLS();
    peers_mutex_destroy();
    return 0;
}


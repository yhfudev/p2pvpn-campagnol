/*
 * Campagnol main code
 *
 * Copyright (C) 2007 Antoine Vianey
 *               2008-2009 Florent Bondoux
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

#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>

#include "communication.h"
#include "pthread_wrap.h"
#include "net_socket.h"
#include "peer.h"
#include "dtls_utils.h"
#include "tun_device.h"
#include "../common/log.h"

struct tb_state global_rate_limiter;

/*
 * Print a message with its content
 */
//void print_smsg(struct message *smsg) {
//    int i, k;
//    char *s;
//    switch(smsg->type) {
//        case HELLO :            s = "| HELLO "; break;
//        case PING :             s = "| PING  "; break;
//        case ASK_CONNECTION :   s = "| ASK   "; break;
//        case PONG :             s = "| PONG  "; break;
//        case OK :               s = "| OK    "; break;
//        case NOK :              s = "| NOK   "; break;
//        case FWD_CONNECTION :   s = "| FWD   "; break;
//        case ANS_CONNECTION :   s = "| ANS   "; break;
//        case REJ_CONNECTION :   s = "| REJ   "; break;
//        case PUNCH :            s = "| PUNCH "; break;
//        case PUNCH_ACK :        s = "| PEER+ "; break;
//        case BYE :              s = "| BYE   "; break;
//        case RECONNECT :        s = "| RECNCT"; break;
//        case CLOSE_CONNECTION :            s = "| CLOSE "; break;
//        default : return; break;
//    }
//
//    printf("*****************************************************\n"
//           "| TYPE  | PORT  | IP 1            | IP 2            |\n");
//    printf("%s", s);
//    // port
//    k = ntohs(smsg->port);
//    printf("| %-6d", k);
//    // ip1
//    char *tmp = inet_ntoa(smsg->ip1);
//    printf("| %s", tmp);
//    for (i=0; i<16-strlen(tmp); i++) printf(" ");
//    // ip2
//    tmp = inet_ntoa(smsg->ip2);
//    printf("| %s", tmp);
//    for (i=0; i<16-strlen(tmp); i++) printf(" ");
//    printf("|\n*****************************************************\n");
//}

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
void handler_sigTimerPing(int sig) {
    message_t smsg;
    /* send a PING message to the RDV server */
    init_smsg(&smsg, PING,0,0);
    int s = sendto(sockfd_global,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
    if (s == -1) log_error("PING");
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
 * Perform the registration of the client to the RDV server
 * sockfd: UDP socket
 */
int register_rdv(int sockfd) {
    message_t smsg, rmsg;
    struct timeval timeout;
    struct sockaddr_in tmp_addr;
    socklen_t tmp_addr_len;

    int s,r;
    int notRegistered = 1;
    int registeringTries = 0;
    fd_set fd_select;

    /* Create a HELLO message */
    log_message_verb("Registering with the RDV server...");
    if (config.send_local_addr == 1) {
        init_smsg(&smsg, HELLO, config.vpnIP.s_addr, config.localIP.s_addr);
        // get the local port
        tmp_addr_len = sizeof(tmp_addr);
        getsockname(sockfd, (struct sockaddr *) &tmp_addr, &tmp_addr_len);
        smsg.port = tmp_addr.sin_port;
    }
    else if (config.send_local_addr == 2) {
        init_smsg(&smsg, HELLO, config.vpnIP.s_addr, config.override_local_addr.sin_addr.s_addr);
        smsg.port = config.override_local_addr.sin_port;
    }
    else {
        init_smsg(&smsg, HELLO, config.vpnIP.s_addr, 0);
    }

    while (notRegistered && registeringTries<MAX_REGISTERING_TRIES && !end_campagnol) {
        /* sending HELLO to the server */
        registeringTries++;
        if (config.debug) printf("Sending HELLO\n");
        if ((s=sendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr))) == -1) {
            log_error("sendto");
        }

        /* fd_set used with the select call */
        FD_ZERO(&fd_select);
        FD_SET(sockfd, &fd_select);
        init_timeout(&timeout);
        select(sockfd+1, &fd_select, NULL, NULL, &timeout);

        if (FD_ISSET(sockfd, &fd_select)!=0 && !end_campagnol) {
            /* Got a message from the server */
            socklen_t len = sizeof(struct sockaddr_in);
            if ( (r = recvfrom(sockfd,&rmsg,sizeof(message_t),0,(struct sockaddr *)&config.serverAddr,&len)) == -1) {
                log_error("recvfrom");
            }
            switch (rmsg.type) {
                case OK:
                    /* Registration OK */
                    log_message_verb("Registration complete");
                    notRegistered = 0;
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
    if (notRegistered) {
        log_message_verb("The connection with the RDV server failed");
        return -1;
    }

    /* set a timer for the ping messages */
    struct itimerval timer_ping;
    timer_ping.it_interval.tv_sec = TIMER_PING_SEC;
    timer_ping.it_interval.tv_usec = TIMER_PING_USEC;
    timer_ping.it_value.tv_sec = TIMER_PING_SEC;
    timer_ping.it_value.tv_usec = TIMER_PING_USEC;
    sockfd_global = sockfd;
    setitimer(ITIMER_REAL, &timer_ping, NULL);

    return 0;
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
    CLIENT_MUTEXLOCK(peer);
    client_update_time(peer, time(NULL));
    /* punching state */
    if (peer->state!=ESTABLISHED) {
        peer->state = PUNCHING;
    }
    CLIENT_MUTEXUNLOCK(peer);
    if (config.verbose) printf("punch %s %d\n", inet_ntoa(peer->clientaddr.sin_addr), ntohs(peer->clientaddr.sin_port));
    for (i=0; i<PUNCH_NUMBER; i++) {
        CLIENT_MUTEXLOCK(peer);
        if (peer->state == CLOSED) {
            CLIENT_MUTEXUNLOCK(peer);
            break;
        }
        CLIENT_MUTEXUNLOCK(peer);
        sendto(peer->sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&(peer->clientaddr), sizeof(peer->clientaddr));
        usleep(PUNCH_DELAY_USEC);
    }
    /* still waiting for an answer */
    CLIENT_MUTEXLOCK(peer);
    if (peer->state!=ESTABLISHED && peer->state!=CLOSED) {
        peer->state = WAITING;
    }
    CLIENT_MUTEXUNLOCK(peer);
    decr_ref(peer);
    pthread_exit(NULL);
}

/*
 * Create a thread executing "punch"
 */
static void start_punch(struct client *peer, int sockfd) {
    incr_ref(peer);
    createDetachedThread(punch, (void *)peer);
}

/*
 * Thread associated with each SSL stream
 *
 * read the outgoing packets from peer->out_fifo
 * and write them to the SSL stream
 */
static void *SSL_writing(void *args) {
    struct client *peer = (struct client *) args;
    int r, w, err;
    int packet_len = MESSAGE_MAX_LENGTH;
    char *packet = CHECK_ALLOC_FATAL(malloc(packet_len));
    while (1) {
        r = BIO_read(peer->out_fifo, packet, packet_len);
        if (r == 0)
            break;
        w = SSL_write(peer->ssl, packet, r);
        if (w <= 0) {
            err = SSL_get_error(peer->ssl, w);
            if (err == SSL_ERROR_ZERO_RETURN)
                break;
        }
    }
    free(packet);
    decr_ref(peer);
    return NULL;
}

/*
 * Create the SSL_writing thread.
 * Must be called when the DTLS session is opened
 */
static void start_SSL_writing(struct client *peer) {
    incr_ref(peer);
    createDetachedThread(SSL_writing, peer);
}

/*
 * Kill the SSL_writing thread
 */
static void end_SSL_writing(struct client *peer) {
    BIO_write(peer->out_fifo, &peer, 0);
}

/*
 * Thread associated with each SSL stream
 *
 * Make the SSL handshake
 * Start the writing thread
 * Loop over SSL_read
 *
 * The SSL stream is fed by calling BIO_write with peer->rbio
 *
 * args: the connected peer (struct client *)
 */
static void * SSL_reading(void * args) {
    int r;
    int err;
    // union used to receive the messages
    packet_t u;
    struct client *peer = (struct client*) args;
    int tunfd = peer->tunfd;

    u.raw = CHECK_ALLOC_FATAL(malloc(MESSAGE_MAX_LENGTH));

    // DTLS handshake
    r = SSL_do_handshake(peer->ssl);
    if (r != 1) {
        log_message("Error during DTLS handshake with peer %s", inet_ntoa(peer->vpnIP));
        ERR_print_errors_fp(stderr);
        message_t smsg;
        init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
        sendto(peer->sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
        CLIENT_MUTEXLOCK(peer);
        peer->thread_ssl_running = 0;
        peer->state = CLOSED;
        CLIENT_MUTEXUNLOCK(peer);
        conditionSignal(&peer->cond_connected);
        decr_ref(peer);
        decr_ref(peer);
        DTLS_MUTEXLOCK;
        ERR_remove_state(0);
        DTLS_MUTEXUNLOCK;
        free(u.raw);
        return NULL;
    }
    /* Unlock the thread waiting for the connection */
    CLIENT_MUTEXLOCK(peer);
    peer->state = ESTABLISHED;
    CLIENT_MUTEXUNLOCK(peer);
    start_SSL_writing(peer);
    conditionSignal(&peer->cond_connected);
    log_message_verb("New DTLS connection opened with peer %s", inet_ntoa(peer->vpnIP));

    while (1) {
        /* Read and uncrypt a message, send it on the TUN device */
        r = SSL_read(peer->ssl, u.raw, MESSAGE_MAX_LENGTH);
        if (r <= 0) { // error or shutdown
            if (BIO_should_read(peer->rbio))
                continue;
            err = SSL_get_error(peer->ssl, r);
            switch(err) {
                case SSL_ERROR_WANT_READ:
                    continue;
                    break;
                case SSL_ERROR_ZERO_RETURN: // shutdown received
                    log_message_verb("DTLS connection closed by peer %s", inet_ntoa(peer->vpnIP));
                    break;
                case SSL_ERROR_SYSCALL: // end_SSL_reading or error
                    if (!peer->thread_ssl_running) { //end_SSL_reading was called
                        log_message_verb("Closing DTLS connection with peer %s", inet_ntoa(peer->vpnIP));
                        if (peer->send_shutdown) {
                            SSL_shutdown(peer->ssl);
                        }
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
            peer->thread_ssl_running = 0;
            peer->state = CLOSED;
            end_SSL_writing(peer);
            CLIENT_MUTEXUNLOCK(peer);
            decr_ref(peer);
            decr_ref(peer);
            DTLS_MUTEXLOCK;
            ERR_remove_state(0);
            DTLS_MUTEXUNLOCK;
            free(u.raw);
            return NULL;
        }
        else {// everything's fine
            CLIENT_MUTEXLOCK(peer);
            client_update_time(peer, time(NULL));
            if (peer->state == TIMEOUT)
                peer->state = ESTABLISHED;
            CLIENT_MUTEXUNLOCK(peer);
            if (config.debug) printf("<< Received a VPN message: size %d from SRC = %u.%u.%u.%u to DST = %u.%u.%u.%u\n",
                            r,
                            (ntohl(u.ip->ip_src.s_addr) >> 24) & 0xFF,
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
            write_tun(tunfd, u.raw, r);
        }
    }
}

/*
 * Create the SSL_reading thread
 */
static void start_SSL_reading(struct client *peer) {
    peer->thread_ssl_running = 1;
    BIO_ctrl(peer->wbio, BIO_CTRL_DGRAM_SET_PEER, 0, &peer->clientaddr);
    incr_ref(peer);
    createDetachedThread(SSL_reading, peer);
}

/*
 * Stop the SSL_reading thread after sending a DTLS shutdown alert
 * AND remove the last reference of the peer
 * The peer is detroyed after calling end_SSL_reading.
 * This insure that there is no inconsistency between the threads
 */
static inline void end_SSL_reading(struct client *peer) {
    peer->thread_ssl_running = 0;
    peer->send_shutdown = 1;
    /* SSL_read will return with 0, which is the same
     * as when we receive a DTLS shutdown alert
     * Just pass a non NULL pointer to BIO_write
     */
    BIO_write(peer->rbio, &peer, 0);
}


/*
 * Manage the incoming messages from the UDP socket
 * argument: struct comm_args *
 */
static void * comm_socket(void * argument) {
    struct comm_args * args = argument;
    int sockfd = args->sockfd;
    int tunfd = args->tunfd;

    int r, s;
    int r_select;
    fd_set fd_select;                           // for the select call
    struct timeval timeout;                     // timeout used with select
    packet_t u;
    struct sockaddr_in unknownaddr;             // address of the sender
    socklen_t len = sizeof(struct sockaddr_in);
    message_t smsg; // outgoing message
    struct client *peer;

    u.raw = CHECK_ALLOC_FATAL(malloc(MESSAGE_MAX_LENGTH));

    time_t timestamp, last_time = 0;

    while (!end_campagnol) {
        /* select call initialisation */
        FD_ZERO(&fd_select);
        FD_SET(sockfd, &fd_select);

        init_timeout(&timeout);
        r_select = select(sockfd+1, &fd_select, NULL, NULL, &timeout);

        timestamp = time(NULL);

        /* MESSAGE READ FROM THE SOCKET */
        if (r_select > 0) {
            r = recvfrom(sockfd,u.raw,MESSAGE_MAX_LENGTH,0,(struct sockaddr *)&unknownaddr,&len);
            /* from the RDV server ? */
            if (config.serverAddr.sin_addr.s_addr == unknownaddr.sin_addr.s_addr
                && config.serverAddr.sin_port == unknownaddr.sin_port) {
                /* which type */
                switch (u.message->type) {
                    /* reject a new session */
                    case REJ_CONNECTION :
                        peer = get_client_VPN(&(u.message->ip1));
                        CLIENT_MUTEXLOCK(peer);
                        peer->state = CLOSED;
                        CLIENT_MUTEXUNLOCK(peer);
                        conditionSignal(&peer->cond_connected);
                        decr_ref(peer);
                        decr_ref(peer);
                        break;
                    /* positive answer */
                    case ANS_CONNECTION :
                        /* get the client */
                        peer = get_client_VPN(&(u.message->ip2));
                        /* complete its informations */
                        CLIENT_MUTEXLOCK(peer);
                        peer->state = PUNCHING;
                        peer->clientaddr.sin_addr = u.message->ip1;
                        peer->clientaddr.sin_port = u.message->port;
                        client_update_time(peer,timestamp);
                        CLIENT_MUTEXUNLOCK(peer);
                        /* and start punching */
                        start_punch(peer, sockfd);
                        decr_ref(peer);
                        break;
                    /* a client wants to open a new session with me */
                    case FWD_CONNECTION :
                        peer = get_client_VPN(&(u.message->ip2));
                        if (peer == NULL) {
                            /* Unknown client, add a new structure */
                            peer = add_client(sockfd, tunfd, PUNCHING, timestamp, u.message->ip1, u.message->port, u.message->ip2, 0);
                            if (peer == NULL) {
                                /* max number of clients */
                                break;
                            }
                            /* start punching */
                            start_punch(peer, sockfd);
                            decr_ref(peer);
                        }
                        else {
                            CLIENT_MUTEXLOCK(peer);
                            if (peer->state == ESTABLISHED) {
                                CLIENT_MUTEXUNLOCK(peer);
                                start_punch(peer, sockfd);
                                decr_ref(peer);
                            }
                            else {
                                CLIENT_MUTEXUNLOCK(peer);
                                decr_ref(peer);
                            }
                        }
                        break;
                    /*
                     * The RDV server want the client to perform a new registration
                     * append if the RDV server restarted
                     */
                    case RECONNECT :
                        register_rdv(sockfd);
                        break;
                    /* a PONG from the RDV server */
                    case PONG :
                        /* ... so it's still up */
                        break;
                    default :
                        break;
                }
            }
            /* Message from another peer */
            else {
                if (config.debug) printf("<  Received a UDP packet: size %d from %s\n", r, inet_ntoa(unknownaddr.sin_addr));
                /* check the first byte to find the message type */
                if (r >= sizeof(dtlsheader_t) &&
                        (u.dtlsheader->contentType == DTLS_APPLICATION_DATA
                        || u.dtlsheader->contentType == DTLS_HANDSHAKE
                        || u.dtlsheader->contentType == DTLS_ALERT
                        || u.dtlsheader->contentType == DTLS_CHANGE_CIPHER_SPEC)) {
                    /* It's a DTLS packet, send it to the associated SSL_reading thread using the FIFO BIO */
                    peer = get_client_real(&unknownaddr);
                    if (peer != NULL) {
                        CLIENT_MUTEXLOCK(peer);
                        if (peer->state == ESTABLISHED || peer->state == PUNCHING || peer->state == TIMEOUT) {
                            CLIENT_MUTEXUNLOCK(peer);
                            BIO_write(peer->rbio, u.raw, r);
                        }
                        else {
                            CLIENT_MUTEXUNLOCK(peer);
                        }
                        decr_ref(peer);
                    }
                }
                else if (r == sizeof(message_t)) {
                    /* UDP hole punching */
                    switch (u.message->type) {
                        case PUNCH :
                            /* we can now reach the client */
                            peer = get_client_real(&unknownaddr);
                            if (peer != NULL) {
                                CLIENT_MUTEXLOCK(peer);
                                if (peer->state != CLOSED) {
                                    if (config.verbose && peer->state != ESTABLISHED) printf("punch received from %s\n", inet_ntoa(u.message->ip1));
                                    client_update_time(peer,timestamp);
                                    if (peer->thread_ssl_running == 0) {
                                        start_SSL_reading(peer);
                                    }
                                    else {
                                        peer->state = ESTABLISHED;
                                        conditionSignal(&peer->cond_connected);
                                    }
                                }
                                CLIENT_MUTEXUNLOCK(peer);
                                decr_ref(peer);
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

        /* check the status of every clients */
        if (timestamp != last_time) {
            last_time = timestamp;
            /* update the state of the clients */
            GLOBAL_MUTEXLOCK;
            peer = clients;
            while (peer != NULL) {
                struct client *next = peer->next;
                CLIENT_MUTEXLOCK(peer);
                if (timestamp - peer->last_keepalive > config.keepalive) {
                    init_smsg(&smsg, PUNCH_KEEP_ALIVE, 0, 0);
                    sendto(sockfd ,&smsg, sizeof(smsg), 0, (struct sockaddr *)&(peer->clientaddr), sizeof(peer->clientaddr));
                    peer->last_keepalive = timestamp;
                }
                if (timestamp - peer->time > config.timeout+10) {
                    log_message_verb("Is the peer %s dead ? cleaning connection", inet_ntoa(peer->vpnIP));
                    if (peer->thread_ssl_running) {
                        init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                        s = sendto(sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                        end_SSL_reading(peer);
                        CLIENT_MUTEXUNLOCK(peer);
                    }
                    else {
                        init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                        s = sendto(sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                        peer->state = CLOSED;
                        CLIENT_MUTEXUNLOCK(peer);
                        decr_ref(peer);
                    }
                }
                else if (timestamp-peer->time>config.timeout) {
                    if (config.debug && peer->state != TIMEOUT ) printf("timeout: %s\n", inet_ntoa(peer->vpnIP));
                    peer->state = TIMEOUT;
                    if (peer->is_dtls_client) {
                        /* close the connection if we started it (client) */
                        if (peer->thread_ssl_running) {
                            init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                            s = sendto(sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                            end_SSL_reading(peer);
                            CLIENT_MUTEXUNLOCK(peer);
                        }
                        else {
                            init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                            s = sendto(sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                            peer->state = CLOSED;
                            CLIENT_MUTEXUNLOCK(peer);
                            decr_ref(peer);
                        }
                    }
                    else {
                        CLIENT_MUTEXUNLOCK(peer);
                    }
                }
                else {
                    CLIENT_MUTEXUNLOCK(peer);
                }
                peer = next;
            }
            GLOBAL_MUTEXUNLOCK;
        }


    }

    free(u.raw);
    DTLS_MUTEXLOCK; // avoid a potential double free bug in OpenSSL's internals
    ERR_remove_state(0);
    DTLS_MUTEXUNLOCK;
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
    fd_set fd_select;                           // for the select call
    struct timeval timeout;                     // timeout used with select
    struct timespec timeout_connect;            // timeout used when opening new connections
    packet_t u;
    struct in_addr dest;
    message_t smsg;
    struct client *peer;

    u.raw = CHECK_ALLOC_FATAL(malloc(MESSAGE_MAX_LENGTH));


    while (!end_campagnol) {
        /* select call initialisation */
        init_timeout(&timeout);
        FD_ZERO(&fd_select);
        FD_SET(tunfd, &fd_select);
        r_select = select(tunfd+1, &fd_select, NULL, NULL, &timeout);


        /* MESSAGE READ FROM TUN DEVICE */
        if (r_select > 0) {
            r = read_tun(tunfd, u.raw, MESSAGE_MAX_LENGTH);
            if (config.debug) printf(">> Sending a VPN message: size %d from SRC = %u.%u.%u.%u to DST = %u.%u.%u.%u\n",
                                r,
                                (ntohl(u.ip->ip_src.s_addr) >> 24) & 0xFF,
                                (ntohl(u.ip->ip_src.s_addr) >> 16) & 0xFF,
                                (ntohl(u.ip->ip_src.s_addr) >> 8) & 0xFF,
                                (ntohl(u.ip->ip_src.s_addr) >> 0) & 0xFF,
                                (ntohl(u.ip->ip_dst.s_addr) >> 24) & 0xFF,
                                (ntohl(u.ip->ip_dst.s_addr) >> 16) & 0xFF,
                                (ntohl(u.ip->ip_dst.s_addr) >> 8) & 0xFF,
                                (ntohl(u.ip->ip_dst.s_addr) >> 0) & 0xFF);
            dest.s_addr = u.ip->ip_dst.s_addr;

            /*
             * If dest IP = VPN broadcast VPN
             */
            if (dest.s_addr == config.vpnBroadcastIP.s_addr) {
                GLOBAL_MUTEXLOCK;
                peer = clients;
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
             * Local packet...
             */
            else if (dest.s_addr == config.vpnIP.s_addr) {
                write_tun(tunfd, u.raw, r);
            }
            else {
                peer = get_client_VPN(&dest);
                if (peer == NULL) {
                    peer = add_client(sockfd, tunfd, TIMEOUT, time(NULL), (struct in_addr) { 0 }, 0, dest, 1);
                    if (peer == NULL) {
                        continue;
                    }
                    /* ask the RDV server for a new connection with peer */
                    init_smsg(&smsg, ASK_CONNECTION, dest.s_addr, 0);
                    sendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));

                    clock_gettime(CLOCK_REALTIME, &timeout_connect);
                    timeout_connect.tv_sec += 3; // wait 3 secs
                    CLIENT_MUTEXLOCK(peer);
                    if (peer->state != CLOSED) {
                        if (conditionTimedwait(&peer->cond_connected, &peer->mutex, &timeout_connect) == 0) {
                            if (peer->state == ESTABLISHED) {
                                CLIENT_MUTEXUNLOCK(peer);
                                BIO_write(peer->out_fifo, u.raw, r);
                            }
                            else {
                                CLIENT_MUTEXUNLOCK(peer);
                            }
                        }
                        else {
                            CLIENT_MUTEXUNLOCK(peer);
                        }
                        decr_ref(peer);
                    }
                    else {
                        CLIENT_MUTEXUNLOCK(peer);
                        decr_ref(peer);
                    }
                }
                else {
                    CLIENT_MUTEXLOCK(peer);
                    switch (peer->state) {
                        /* Already connected */
                        case ESTABLISHED :
                            client_update_time(peer,time(NULL));
                            CLIENT_MUTEXUNLOCK(peer);
                            BIO_write(peer->out_fifo, u.raw, r);
                            decr_ref(peer);
                            break;
                        /* Lost connection */
                        case TIMEOUT :
                            /* try to reopen the connection */
                            /* ask the RDV server for a new connection with peer */
                            init_smsg(&smsg, ASK_CONNECTION, dest.s_addr, 0);
                            sendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));

                            clock_gettime(CLOCK_REALTIME, &timeout_connect);
                            timeout_connect.tv_sec += 3;
                            if (conditionTimedwait(&peer->cond_connected, &peer->mutex, &timeout_connect) == 0) {
                                if (peer->state == ESTABLISHED) {
                                    CLIENT_MUTEXUNLOCK(peer);
                                    BIO_write(peer->out_fifo, u.raw, r);
                                }
                                else {
                                    CLIENT_MUTEXUNLOCK(peer);
                                }
                                decr_ref(peer);
                            }
                            else {
                                if (peer->thread_ssl_running) {
                                    end_SSL_reading(peer);
                                }
                                CLIENT_MUTEXUNLOCK(peer);
                                decr_ref(peer);
                            }
                            break;
                        case WAITING:
                            if (!peer->thread_ssl_running) {
                                // initial connection failed, kill.
                                decr_ref(peer);
                            }
                            CLIENT_MUTEXUNLOCK(peer);
                            decr_ref(peer);
                            break;
                        default:
                            CLIENT_MUTEXUNLOCK(peer);
                            decr_ref(peer);
                            break;
                    }
                }
            }
        }

    }

    free(u.raw);
    DTLS_MUTEXLOCK; // avoid a potential double free bug in OpenSSL's internals
    ERR_remove_state(0);
    DTLS_MUTEXUNLOCK;
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
    struct comm_args *args = (struct comm_args*) CHECK_ALLOC_FATAL(malloc(sizeof(struct comm_args)));
    args->sockfd = sockfd;
    args->tunfd = tunfd;

    /* initialize the global rate limiter */
    if (config.tb_client_size != 0) {
        tb_init(&global_rate_limiter, config.tb_client_size, (double) config.tb_client_rate, 8, 1);
    }

    mutex_clients_init();
    if (initDTLS() == -1) {
        return -1;
    }

    pthread_t th_socket, th_tun;
    th_socket = createThread(comm_socket, args);
    th_tun = createThread(comm_tun, args);

    joinThread(th_socket, NULL);
    joinThread(th_tun, NULL);

    free(args);

    GLOBAL_MUTEXLOCK;
    struct client *peer, *next;
    peer = clients;
    while (peer != NULL) {
        next = peer->next;
        CLIENT_MUTEXLOCK(peer);
        if (peer->thread_ssl_running) {
            end_SSL_reading(peer);
            CLIENT_MUTEXUNLOCK(peer);
            // don't wait for the SSL_reading thread terminaison here !
            // the thread needs to lock the mutex
        }
        else {
            peer->state = CLOSED;
            CLIENT_MUTEXUNLOCK(peer);
            decr_ref(peer);
        }
        peer = next;
    }
    GLOBAL_MUTEXUNLOCK;

    init_smsg(&smsg, BYE, config.vpnIP.s_addr, 0);
    if (config.debug) printf("Sending BYE\n");
    if (sendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr)) == -1) {
        log_error("sendto");
    }

    // wait for all the SSL_reading thread to finish
    while (n_clients != 0) { usleep(100000); }

    if (config.tb_client_size != 0) {
        tb_clean(&global_rate_limiter);
    }

    clearDTLS();
    mutex_clients_destroy();
    return 0;
}


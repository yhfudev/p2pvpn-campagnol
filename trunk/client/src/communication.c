/*
 * Campagnol main code
 *
 * Copyright (C) 2007 Antoine Vianey
 *               2008 Florent Bondoux
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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>

#include "communication.h"
#include "pthread_wrap.h"
#include "net_socket.h"
#include "peer.h"
#include "log.h"


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
inline void init_smsg(struct message *smsg, int type, u_int32_t ip1, u_int32_t ip2) {
    bzero(smsg, sizeof(struct message));
    smsg->type = type;
    smsg->ip1.s_addr = ip1;
    smsg->ip2.s_addr = ip2;
}

/* Initialise a timeval structure to use with select */
inline void init_timeout(struct timeval *timeout) {
    timeout->tv_sec = SELECT_DELAY_SEC;
    timeout->tv_usec = SELECT_DELAY_USEC;
}

/*
 * Handler for SIGALRM
 * send a PING message
 */
static int sockfd_global;
void handler_sigTimerPing(int sig) {
    struct message smsg;
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
uint16_t compute_csum(uint16_t *addr, size_t count){
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

    return sum;
}

/*
 * Perform the registration of the client to the RDV server
 * sockfd: UDP socket
 */
int register_rdv(int sockfd) {
    struct message smsg, rmsg;
    struct timeval timeout;

    int s,r;
    int notRegistered = 1;
    int registeringTries = 0;
    fd_set fd_select;

    /* Create a HELLO message */
    log_message_verb("Registering with the RDV server...");
    init_smsg(&smsg, HELLO, config.vpnIP.s_addr, 0);

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
            if ( (r = recvfrom(sockfd,&rmsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr,&len)) == -1) {
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
        return 1;
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
void *punch(void *arg) {
    int i;
    struct client *peer = (struct client *)arg;
    struct message smsg;
    init_smsg(&smsg, PUNCH, config.vpnIP.s_addr, 0);
    peer->time = time(NULL);
    /* punching state */
    MUTEXLOCK;
    if (peer->state!=ESTABLISHED) {
        peer->state = PUNCHING;
    }
    MUTEXUNLOCK;
    if (config.verbose) printf("punch %s %d\n", inet_ntoa(peer->clientaddr.sin_addr), ntohs(peer->clientaddr.sin_port));
    for (i=0; i<PUNCH_NUMBER && peer->state != CLOSED; i++) {
        sendto(peer->sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&(peer->clientaddr), sizeof(peer->clientaddr));
        usleep(PUNCH_DELAY_USEC);
    }
    /* still waiting for an answer */
    MUTEXLOCK;
    if (peer->state!=ESTABLISHED && peer->state!=CLOSED) {
        peer->state = WAITING;
    }
    decr_ref(peer);
    MUTEXUNLOCK;
    pthread_exit(NULL);
}

/*
 * Create a thread executing "punch"
 */
void start_punch(struct client *peer, int sockfd) {
    pthread_t t;
    incr_ref(peer);
    t = createThread(punch, (void *)peer);
}

/*
 * Thread associated with each SSL stream
 *
 * Make the SSL handshake
 * Loop over SSL_read
 *
 * The SSL stream is fed by calling BIO_write with peer->rbio
 *
 * args: the connected peer (struct client *)
 */
void * SSL_reading(void * args) {
    int r;
    // union used to receive the messages
    union {
        struct ip ip;
        unsigned char raw[MESSAGE_MAX_LENGTH];
    } u;
    struct client *peer = (struct client*) args;
    int tunfd = peer->tunfd;

    // DTLS handshake
    r = peer->ssl->handshake_func(peer->ssl);
    if (r != 1) {
        log_message("Error during DTLS handshake with peer %s", inet_ntoa(peer->vpnIP));
        ERR_print_errors_fp(stderr);
        struct message smsg;
        init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
        sendto(peer->sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
        MUTEXLOCK;
        peer->state = CLOSED;
        conditionSignal(&peer->cond_connected);
        decr_ref(peer);
        decr_ref(peer);
        MUTEXUNLOCK;
        return NULL;
    }
    /* Unlock the thread waiting for the connection */
    MUTEXLOCK;
    peer->state = ESTABLISHED;
    MUTEXUNLOCK;
    conditionSignal(&peer->cond_connected);
    log_message_verb("new DTLS connection opened with peer %s", inet_ntoa(peer->vpnIP));

    while (1) {
        /* Read and uncrypt a message, send it on the TUN device */
        r = SSL_read(peer->ssl, &u, MESSAGE_MAX_LENGTH);
        if (r < 0) { // error
            if (BIO_should_read(peer->rbio)) {
                continue;
            }
            int err = SSL_get_error(peer->ssl, r);
            log_message("SSL_read %d %d", err, errno);
            continue;
        }
        MUTEXLOCK;
        if (r == 0) { // close connection
            log_message_verb("DTLS connection closed with peer %s", inet_ntoa(peer->vpnIP));
            if (peer->send_shutdown) {
                SSL_shutdown(peer->ssl);
            }
            peer->thread_running = 0;
            peer->state = CLOSED;
            decr_ref(peer);
            decr_ref(peer);
            MUTEXUNLOCK;
            return NULL;
        }
        else {// everything's fine
            peer->time = time(NULL);
            peer->state = ESTABLISHED;
            MUTEXUNLOCK;
            if (config.debug) printf("<< Received a VPN message: size %d from SRC = %u.%u.%u.%u to DST = %u.%u.%u.%u\n",
                            r,
                            (ntohl(u.ip.ip_src.s_addr) >> 24) & 0xFF,
                            (ntohl(u.ip.ip_src.s_addr) >> 16) & 0xFF,
                            (ntohl(u.ip.ip_src.s_addr) >> 8) & 0xFF,
                            (ntohl(u.ip.ip_src.s_addr) >> 0) & 0xFF,
                            (ntohl(u.ip.ip_dst.s_addr) >> 24) & 0xFF,
                            (ntohl(u.ip.ip_dst.s_addr) >> 16) & 0xFF,
                            (ntohl(u.ip.ip_dst.s_addr) >> 8) & 0xFF,
                            (ntohl(u.ip.ip_dst.s_addr) >> 0) & 0xFF);
            /*
             * If dest IP = VPN broadcast VPN
             * The TUN device creates a point to point connection which
             * does not transmit the broadcast IP
             * So alter the dest. IP to the normal VPN IP
             * and compute the new checksum
             */
            if (u.ip.ip_dst.s_addr == config.vpnBroadcastIP.s_addr) {
                u.ip.ip_dst.s_addr = config.vpnIP.s_addr;
                u.ip.ip_sum = 0; // the checksum field is set to 0 for the calculation
                u.ip.ip_sum = ~compute_csum((uint16_t*) &u.ip, sizeof(u.ip));
            }
            // send it to the TUN device
            write(tunfd, (unsigned char *)&u, sizeof(u));
        }
    }
}

/*
 * Create the SSL_reading thread
 */
void start_SSL_reading(struct client *peer) {
    peer->thread_running = 1;
    incr_ref(peer);
    peer->thread = createThread(SSL_reading, peer);
}

/*
 * Stop the SSL_reading thread after sending a DTLS shutdown alert
 * AND remove the last reference of the peer
 * The peer is detroyed after calling end_SSL_reading.
 * This insure that there is no inconsistency between the threads
 */
inline void end_SSL_reading(struct client *peer) {
    peer->thread_running = 0;
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
void * comm_socket(void * argument) {
    struct comm_args * args = argument;
    int sockfd = args->sockfd;
    int tunfd = args->tunfd;

    int r, s;
    int r_select;
    fd_set fd_select;                           // for the select call
    struct timeval timeout;                     // timeout used with select
    union {                                     // incoming messages
        struct {
            unsigned char contentType;          // same as struct message's type (unsigned char)
            unsigned char protocolVersionMaj;
            unsigned char protocolVersionMin;
        } dtlsheader;
        unsigned char raw[MESSAGE_MAX_LENGTH];
    } u;
    struct sockaddr_in unknownaddr;             // address of the sender
    socklen_t len = sizeof(struct sockaddr_in);
    struct message *rmsg = (struct message *) &u;   // incoming struct message
    struct message smsg; // outgoing message
    struct client *peer;

    init_timeout(&timeout);

    while (!end_campagnol) {
        /* select call initialisation */
        FD_ZERO(&fd_select);
        FD_SET(sockfd, &fd_select);

        r_select = select(sockfd+1, &fd_select, NULL, NULL, &timeout);

        /* MESSAGE READ FROM THE SOCKET */
        if (r_select > 0) {
            r = recvfrom(sockfd,(unsigned char *)&u,MESSAGE_MAX_LENGTH,0,(struct sockaddr *)&unknownaddr,&len);
            /* from the RDV server ? */
            if (config.serverAddr.sin_addr.s_addr == unknownaddr.sin_addr.s_addr
                && config.serverAddr.sin_port == unknownaddr.sin_port) {
                /* which type */
                switch (rmsg->type) {
                    /* reject a new session */
                    case REJ_CONNECTION :
                        MUTEXLOCK;
                        peer = get_client_VPN(&(rmsg->ip1));
                        peer->state = CLOSED;
                        conditionSignal(&peer->cond_connected);
                        decr_ref(peer);
                        decr_ref(peer);
                        MUTEXUNLOCK;
                        break;
                    /* positive answer */
                    case ANS_CONNECTION :
                        /* get the client */
                        MUTEXLOCK;
                        peer = get_client_VPN(&(rmsg->ip2));
                        /* complete its informations */
                        peer->state = PUNCHING;
                        peer->clientaddr.sin_addr = rmsg->ip1;
                        peer->clientaddr.sin_port = rmsg->port;
                        peer->time = time(NULL);
                        MUTEXUNLOCK;
                        /* and start punching */
                        start_punch(peer, sockfd);
                        decr_ref(peer);
                        break;
                    /* a client wants to open a new session with me */
                    case FWD_CONNECTION :
                        MUTEXLOCK;
                        if (get_client_VPN(&(rmsg->ip2)) == NULL) {
                            /* Unknown client, add a new structure */
                            peer = add_client(sockfd, tunfd, PUNCHING, time(NULL), rmsg->ip1, rmsg->port, rmsg->ip2, 0);
                            if (peer == NULL) {
                                /* max number of clients */
                                MUTEXUNLOCK;
                                break;
                            }
                            /* start punching */
                            start_punch(peer, sockfd);
                            decr_ref(peer);
                        }
                        MUTEXUNLOCK;
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
                if (u.dtlsheader.contentType == DTLS_APPLICATION_DATA
                        || u.dtlsheader.contentType == DTLS_HANDSHAKE
                        || u.dtlsheader.contentType == DTLS_ALERT
                        || u.dtlsheader.contentType == DTLS_CHANGE_CIPHER_SPEC) {
                    /* It's a DTLS packet, send it to the associated SSL_reading thread using the FIFO BIO */
                    MUTEXLOCK;
                    peer = get_client_real(&unknownaddr);
                    MUTEXUNLOCK;
                    if (peer != NULL) {
                        if (peer->state != CLOSED) {
                            BIO_write(peer->rbio, &u, r);
                        }
                        decr_ref(peer);
                    }
                }
                else if (r == sizeof(struct message)) {
                    /* UDP hole punching */
                    switch (rmsg->type) {
                        case PUNCH :
                        case PUNCH_ACK :
                            /* we can now reach the client */
                            MUTEXLOCK;
                            peer = get_client_VPN(&(rmsg->ip1));
                            if (peer != NULL) {
                                if (config.verbose && peer->state != ESTABLISHED) printf("punch received from %s\n", inet_ntoa(rmsg->ip1));
                                peer->time = time(NULL);
                                peer->clientaddr = unknownaddr;
                                if (peer->thread_running == 0) {
                                    BIO_ctrl(peer->wbio, BIO_CTRL_DGRAM_SET_PEER, 0, &peer->clientaddr);
                                    start_SSL_reading(peer);
                                }
                                decr_ref(peer);
                            }
                            MUTEXUNLOCK;
                            break;
                        default :
                            break;
                    }
                }
            }


        }


        /* TIMEOUT */
        else if (r_select == 0) {
            init_timeout(&timeout);
            /* update the state of the clients */
            MUTEXLOCK;
            peer = clients;
            while (peer != NULL) {
                struct client *next = peer->next;
                if (time(NULL) - peer->time > config.timeout+10) {
                    log_message_verb("Is the peer %s dead ? cleaning connection", inet_ntoa(peer->vpnIP));
                    if (peer->thread_running) {
                        init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                        s = sendto(sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                        end_SSL_reading(peer);
                    }
                    else {
                        init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                        s = sendto(sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                        peer->state = CLOSED;
                        decr_ref(peer);
                    }
                }
                else if (time(NULL)-peer->time>config.timeout) {
                    if (config.debug && peer->state != TIMEOUT ) printf("timeout: %s\n", inet_ntoa(peer->vpnIP));
                    peer->state = TIMEOUT;
                    if (peer->is_dtls_client) {
                        /* close the connection if we started it (client) */
                        if (peer->thread_running) {
                            init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                            s = sendto(sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                            end_SSL_reading(peer);
                        }
                        else {
                            init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                            s = sendto(sockfd, &smsg, sizeof(smsg), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                            peer->state = CLOSED;
                            decr_ref(peer);
                        }
                    }
                }
                peer = next;
            }
            MUTEXUNLOCK;
        }


    }

    return NULL;
}


/*
 * Manage the incoming messages from the TUN device
 * argument: struct comm_args *
 */
void * comm_tun(void * argument) {
    struct comm_args * args = argument;
    int sockfd = args->sockfd;
    int tunfd = args->tunfd;

    int r;
    int r_select;
    fd_set fd_select;                           // for the select call
    struct timeval timeout;                     // timeout used with select
    struct timespec timeout_connect;            // timeout used when opening new connections
    union {
        struct ip ip;
        unsigned char raw[MESSAGE_MAX_LENGTH];
    } u;
    struct in_addr dest;
    struct message smsg;
    struct client *peer;


    while (!end_campagnol) {
        /* select call initialisation */
        init_timeout(&timeout);
        FD_ZERO(&fd_select);
        FD_SET(tunfd, &fd_select);
        r_select = select(tunfd+1, &fd_select, NULL, NULL, &timeout);


        /* MESSAGE READ FROM TUN DEVICE */
        if (r_select > 0) {
            r = read(tunfd, &u, sizeof(u));
            if (config.debug) printf(">> Sending a VPN message: size %d from SRC = %u.%u.%u.%u to DST = %u.%u.%u.%u\n",
                                r,
                                (ntohl(u.ip.ip_src.s_addr) >> 24) & 0xFF,
                                (ntohl(u.ip.ip_src.s_addr) >> 16) & 0xFF,
                                (ntohl(u.ip.ip_src.s_addr) >> 8) & 0xFF,
                                (ntohl(u.ip.ip_src.s_addr) >> 0) & 0xFF,
                                (ntohl(u.ip.ip_dst.s_addr) >> 24) & 0xFF,
                                (ntohl(u.ip.ip_dst.s_addr) >> 16) & 0xFF,
                                (ntohl(u.ip.ip_dst.s_addr) >> 8) & 0xFF,
                                (ntohl(u.ip.ip_dst.s_addr) >> 0) & 0xFF);
            dest.s_addr = u.ip.ip_dst.s_addr;

            /*
             * If dest IP = VPN broadcast VPN
             */
            if (dest.s_addr == config.vpnBroadcastIP.s_addr) {
                MUTEXLOCK;
                peer = clients;
                while (peer != NULL) {
                    struct client *next = peer->next;
                    if (peer->state == ESTABLISHED) {
                        SSL_write(peer->ssl, &u, r);
                    }
                    peer = next;
                }
                MUTEXUNLOCK;
            }
            else {
                MUTEXLOCK;
                peer = get_client_VPN(&dest);
                MUTEXUNLOCK;
                if (peer == NULL) {
                    MUTEXLOCK;
                    peer = add_client(sockfd, tunfd, TIMEOUT, time(NULL), (struct in_addr) { 0 }, 0, dest, 1);
                    MUTEXUNLOCK;
                    if (peer == NULL) {
                        continue;
                    }
                    /* ask the RDV server for a new connection with peer */
                    init_smsg(&smsg, ASK_CONNECTION, dest.s_addr, 0);
                    sendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));

                    clock_gettime(CLOCK_REALTIME, &timeout_connect);
                    timeout_connect.tv_sec += 3; // wait 3 secs
                    MUTEXLOCK;
                    if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                        decr_ref(peer);
                        peer = get_client_VPN(&dest);
                        if (peer != NULL) {// the new connection is opened
                            if (peer->state == ESTABLISHED) {
                                SSL_write(peer->ssl, &u, r);
                            }
                            decr_ref(peer);
                        }
                    }
                    else {
                        decr_ref(peer);
                    }
                    MUTEXUNLOCK;

                }
                else switch (peer->state) {
                    /* Already connected */
                    case ESTABLISHED :
                        MUTEXLOCK;
                        peer->time = time(NULL);
                        MUTEXUNLOCK;
                        SSL_write(peer->ssl, &u, r);
                        decr_ref(peer);
                        break;
                    /* Lost connection */
                    case TIMEOUT :
                        /* try to reopen the connection */
                        MUTEXLOCK;
                        peer->time = time(NULL);
                        MUTEXUNLOCK;
                        /* ask the RDV server for a new connection with peer */
                        init_smsg(&smsg, ASK_CONNECTION, dest.s_addr, 0);
                        sendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));

                        clock_gettime(CLOCK_REALTIME, &timeout_connect);
                        timeout_connect.tv_sec += 3;
                        MUTEXLOCK;
                        if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                            decr_ref(peer);
                            peer = get_client_VPN(&dest);
                            if (peer != NULL) {// the new connection is opened
                                if (peer->state == ESTABLISHED) {
                                    SSL_write(peer->ssl, &u, r);
                                }
                                decr_ref(peer);
                            }
                        }
                        else {
                            decr_ref(peer);
                        }
                        MUTEXUNLOCK;
                        break;
                    /* PUNCHING or WAITING
                     * The connection is in progress
                     */
                    case PUNCHING:
                    case WAITING:
                        clock_gettime(CLOCK_REALTIME, &timeout_connect);
                        timeout_connect.tv_sec += 2; // wait another 2 secs
                        MUTEXLOCK;
                        if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                            decr_ref(peer);
                            peer = get_client_VPN(&dest);
                            if (peer != NULL) {// the new connection is opened
                                if (peer->state == ESTABLISHED) {
                                    SSL_write(peer->ssl, &u, r);
                                }
                                decr_ref(peer);
                            }
                        }
                        else {
                            decr_ref(peer);
                        }
                        MUTEXUNLOCK;
                        break;
                    default:
                        decr_ref(peer);
                        break;
                }
            }
        }

    }

    return NULL;
}


/*
 * Start the VPN:
 * start a thread running comm_tun and another one running comm_socket
 *
 * set end_campagnol to 1 un order to stop both threads (and others)
 */
void start_vpn(int sockfd, int tunfd) {
    struct message smsg;
    struct comm_args *args = (struct comm_args*) malloc(sizeof(struct comm_args));
    if (args == NULL) {
        log_error("Could not allocate structure (start_vpn)");
        exit(EXIT_FAILURE);
    }
    args->sockfd = sockfd;
    args->tunfd = tunfd;

    SSL_library_init();
    SSL_load_error_strings();

    mutex_clients_init();

    pthread_t th_socket, th_tun;
    th_socket = createThread(comm_socket, args);
    th_tun = createThread(comm_tun, args);

    joinThread(th_socket, NULL);
    joinThread(th_tun, NULL);

    free(args);

    MUTEXLOCK;
    struct client *peer, *next;
    peer = clients;
    while (peer != NULL) {
        next = peer->next;
        if (peer->thread_running) {
            end_SSL_reading(peer);
            // don't wait for the SSL_reading thread terminaison here !
            // it requires to lock the mutex
        }
        else {
            peer->state = CLOSED;
            decr_ref(peer);
        }
        peer = next;
    }
    MUTEXUNLOCK;

    init_smsg(&smsg, BYE, config.vpnIP.s_addr, 0);
    if (config.debug) printf("Sending BYE\n");
    if (sendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr)) == -1) {
        log_error("sendto");
    }

    // wait for all the SSL_reading thread to finish
    while (n_clients != 0) { usleep(100000); }

    mutex_clients_destroy();
}

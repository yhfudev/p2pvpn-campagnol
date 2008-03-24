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


#include <time.h> 
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "campagnol.h"
#include "communication.h"
#include "pthread_wrap.h"
#include "net_socket.h"
#include "peer.h"


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
    if (config.verbose) printf("Registering with the RDV server...\n");
    init_smsg(&smsg, HELLO, config.vpnIP.s_addr, 0);
    
    while (notRegistered && registeringTries<MAX_REGISTERING_TRIES && !end_campagnol) {
        /* sending HELLO to the server */
        registeringTries++;
        if (config.debug) printf("Sending HELLO\n");
        if ((s=sendto(sockfd,&smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr))) == -1) {
            perror("sendto");
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
                perror("recvfrom");
            }
            switch (rmsg.type) {
                case OK:
                    /* Registration OK */
                    if (config.verbose) printf("Registration complete\n");
                    notRegistered = 0;
                    return 0;
                    break;
                case NOK:
                    /* The RDV server rejected the client */
                    fprintf(stderr, "The RDV server rejected the client\n");
                    sleep(1);
                    break;
                default:
                    fprintf(stderr, "The RDV server replied with something strange... (message code is %d)\n", rmsg.type);
                    break;
            }
        }
    }

    /* Registration failed. The server may be down */
    if (notRegistered) {
        fprintf(stderr, "The connection with the RDV server failed\n");
        return 1;
    }
    return 0;
}


/* 
 * Function sending the punch messages for UDP hole punching
 * arg: struct punch_arg
 */
void *punch(void *arg) {
    int i;
    struct punch_arg *str = (struct punch_arg *)arg;
    struct message smsg;
    init_smsg(&smsg, PUNCH, config.vpnIP.s_addr, 0);
    str->peer->time = time(NULL);
    /* punching state */
    MUTEXLOCK;
    if (str->peer->state!=ESTABLISHED) {
        str->peer->state = PUNCHING;
    }
    MUTEXUNLOCK;
    if (config.verbose) printf("punch %s %d\n", inet_ntoa(str->peer->clientaddr.sin_addr), ntohs(str->peer->clientaddr.sin_port));
    for (i=0; i<PUNCH_NUMBER; i++) {
        sendto(*(str->sockfd),&smsg,sizeof(struct message),0,(struct sockaddr *)&(str->peer->clientaddr), sizeof(str->peer->clientaddr));
        usleep(PUNCH_DELAY_USEC);
    }
    /* still waiting for an answer */
    MUTEXLOCK;
    if (str->peer->state!=ESTABLISHED) {
        str->peer->state = WAITING;
    }
    MUTEXUNLOCK;
    free(arg);
    pthread_exit(NULL);
}

/*
 * Create a thread executing "punch"
 */
void start_punch(struct client *peer, int *sockfd) {
    struct punch_arg *arg = malloc(sizeof(struct punch_arg));
    arg->sockfd = sockfd;
    arg->peer = peer;
    pthread_t t;
    t = createThread(punch, (void *)arg);
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
        struct {
            struct iphdr ip;
        } fmt;
        unsigned char raw[MESSAGE_MAX_LENGTH];
    } u; 
    struct client *peer = (struct client*) args;
    int tunfd = peer->tunfd;
    
    // DTLS handshake
    r = peer->ssl->handshake_func(peer->ssl);
    if (r != 1) {
        fprintf(stderr, "Error during DTLS handshake with peer %s\n", inet_ntoa(peer->vpnIP));
        ERR_print_errors_fp(stderr);
        MUTEXLOCK;
        remove_client(peer);
        MUTEXUNLOCK;
        struct message smsg;
        init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
        sendto(peer->sockfd, &smsg, sizeof(struct message), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
        conditionSignal(&peer->cond_connected);
        return NULL;
    }
    /* Unlock the thread waiting for the connection */
    conditionSignal(&peer->cond_connected);
    if (config.verbose) printf("new DTLS connection opened with peer %s\n", inet_ntoa(peer->vpnIP));
    
    while (!end_campagnol) {
        /* Read and uncrypt a message, send it on the TUN device */
        r = SSL_read(peer->ssl, &u, MESSAGE_MAX_LENGTH);
        if (r < 0) { // error
            int err = SSL_get_error(peer->ssl, r);
            fprintf(stderr, "SSL_read %d\n", err);
            ERR_print_errors_fp(stderr);
        }
        MUTEXLOCK;
        if (r == 0) { // close connection
            if (config.verbose) printf("DTLS connection closed with peer %s\n", inet_ntoa(peer->vpnIP));
            if (end_campagnol) {
                MUTEXUNLOCK;
                return NULL;
            }
            peer->state = TIMEOUT;
            peer->thread_running = 0;
            //createClientSSL(peer, 1); // rather remove entirely the client structure than recreate it
            remove_client(peer);
            MUTEXUNLOCK;
            return NULL;
        }
        else {// everything's fine
            peer->time = time(NULL);
            peer->state = ESTABLISHED;
            MUTEXUNLOCK;
            if (config.debug) printf("<< Received a VPN message: size %d from SRC = %u.%u.%u.%u to DST = %u.%u.%u.%u\n",
                            r,
                            (ntohl(u.fmt.ip.saddr) >> 24) & 0xFF,
                            (ntohl(u.fmt.ip.saddr) >> 16) & 0xFF,
                            (ntohl(u.fmt.ip.saddr) >> 8) & 0xFF,
                            (ntohl(u.fmt.ip.saddr) >> 0) & 0xFF,
                            (ntohl(u.fmt.ip.daddr) >> 24) & 0xFF,
                            (ntohl(u.fmt.ip.daddr) >> 16) & 0xFF,
                            (ntohl(u.fmt.ip.daddr) >> 8) & 0xFF,
                            (ntohl(u.fmt.ip.daddr) >> 0) & 0xFF);
            // send it to the TUN device
            write(tunfd, (unsigned char *)&u, sizeof(u));
        }
    }
    return NULL;
}

/*
 * Create the SSL_reading thread
 */
void createSSL(struct client *peer) {
    peer->thread_running = 1;
    peer->thread = createThread(SSL_reading, peer);
}


/*
 * Manage the incoming messages from the UDP socket
 * argument: struct comm_args *
 */
void * comm_socket(void * argument) {
    struct comm_args * args = argument;
    int sockfd = args->sockfd;
    int tunfd = args->tunfd;
    
    int r;
    int r_select;
    fd_set fd_select;                           // for the select call
    struct timeval timeout;                     // timeout used with select
    union {                                     // incoming messages
        struct {
            struct iphdr ip;
        } fmt;
        unsigned char raw[MESSAGE_MAX_LENGTH];
    } u;
    struct sockaddr_in unknownaddr;             // address of the sender
    socklen_t len = sizeof(struct sockaddr_in);
    struct message *rmsg = (struct message *) &u;   // incoming struct message
    struct message *smsg = (struct message *) malloc(sizeof(struct message)); // outgoing message
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
                        remove_client(peer);
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
                        start_punch(peer, &sockfd);
                        break;
                    /* a client wants to open a new session with me */
                    case FWD_CONNECTION :
                        MUTEXLOCK;
                        if (get_client_VPN(&(rmsg->ip2)) == NULL) {
                            /* Unknown client, add a new structure */
                            peer = add_client(sockfd, tunfd, PUNCHING, time(NULL), rmsg->ip1, rmsg->port, rmsg->ip2, 0);
                        }
                        /* otherwise adjust the data */
                        else {
                            peer = get_client_VPN(&(rmsg->ip2));
                            peer->state = PUNCHING;
                            peer->clientaddr.sin_addr = rmsg->ip1;
                            peer->clientaddr.sin_port = rmsg->port;
                            peer->time = time(NULL);
                        }
                        MUTEXUNLOCK;
                        /* start punching */
                        start_punch(peer, &sockfd);
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
                if (config.debug) printf("<< Received a packet from a VPN client, size %d\n", r);
                /* UDP hole punching */
                /* TODO: should rather check if its an IPv4 message or something other.
                 *       And then check if it's a PUNCH message
                 */
                if (rmsg->type == PUNCH || rmsg->type == PUNCH_ACK) {
                    switch (rmsg->type) {
                        case PUNCH :
                        case PUNCH_ACK :
                            /* we can now reach the client */
                            MUTEXLOCK;
                            peer = get_client_VPN(&(rmsg->ip1));
                            if (peer != NULL) { // TODO: if a punch message comes before a FWD_CONNECTION ?
                                if (config.verbose && peer->state != ESTABLISHED) printf("punch received from %s\n", inet_ntoa(rmsg->ip1));
                                peer->time = time(NULL);
                                peer->state = ESTABLISHED;
                                peer->clientaddr = unknownaddr;
                                if (peer->thread_running == 0) {
                                    BIO_ctrl(peer->wbio, BIO_CTRL_DGRAM_SET_PEER, 0, &peer->clientaddr);
                                    createSSL(peer);
                                }
                            }
                            MUTEXUNLOCK;
                            break;
                        default :
                            break;
                    }
                }
                /* It's an IP packet, send it to the associated SSL_reading thread using the FIFO BIO */
                else {
                    MUTEXLOCK;
                    peer = get_client_real(&unknownaddr);
                    MUTEXUNLOCK;
                    if (peer != NULL) {
                        BIO_write(peer->rbio, &u, r);
                    }
                }
            }
            
        
        }
        
        
        /* TIMEOUT */
        else if (r_select == 0) {
            /* send a PING message to the RDV server */
            init_smsg(smsg, PING,0,0);
            init_timeout(&timeout);
            int s = sendto(sockfd,smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
            if (s == -1) perror("PING");
            /* update the state of the clients */
            MUTEXLOCK;
            peer = clients;
            while (peer != NULL) {
                struct client *next = peer->next;
                if (time(NULL)-peer->time>config.timeout) {
                    peer->state = TIMEOUT;
                    if (peer->ssl != NULL) {
                        if (config.debug) printf("timeout: %s\n", inet_ntoa(peer->vpnIP));
                        /* close the connection if we started it (client) */                   
                        if (peer->is_dtls_client && peer->thread_running) {
                            peer->thread_running = 0;
                            SSL_shutdown(peer->ssl);
                            BIO_write(peer->rbio, &u, 0);
                            init_smsg(smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                            s = sendto(sockfd, smsg, sizeof(struct message), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                        }
                    }
                }
                peer = next;
            }
            MUTEXUNLOCK;
        }
    
    
    }
    
    free(smsg);
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
        struct {
            struct iphdr ip;
        } fmt;
        unsigned char raw[MESSAGE_MAX_LENGTH];
    } u;
    struct in_addr dest;
    struct message *smsg = (struct message *) malloc(sizeof(struct message));
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
                                (ntohl(u.fmt.ip.saddr) >> 24) & 0xFF,
                                (ntohl(u.fmt.ip.saddr) >> 16) & 0xFF,
                                (ntohl(u.fmt.ip.saddr) >> 8) & 0xFF,
                                (ntohl(u.fmt.ip.saddr) >> 0) & 0xFF,
                                (ntohl(u.fmt.ip.daddr) >> 24) & 0xFF,
                                (ntohl(u.fmt.ip.daddr) >> 16) & 0xFF,
                                (ntohl(u.fmt.ip.daddr) >> 8) & 0xFF,
                                (ntohl(u.fmt.ip.daddr) >> 0) & 0xFF);
            dest.s_addr = u.fmt.ip.daddr;

            /*
             * If dest IP = VPN broadcast VPN
             * The TUN device creates a point to point connection which
             * does not transmit the broadcast IP
             * So alter the dest. IP to the normal VPN IP
             * and compute the new checksum
             * TODO: it should be on the receiver side
             */
            if (dest.s_addr == config.vpnBroadcastIP.s_addr) {
                MUTEXLOCK;
                peer = clients;
                while (peer != NULL) {
                    struct client *next = peer->next;
                    if (peer->state == ESTABLISHED) {
                        u.fmt.ip.daddr = peer->vpnIP.s_addr;
                        u.fmt.ip.check = 0; // the checksum field is set to 0 for the calculation
                        u.fmt.ip.check = ~compute_csum((uint16_t*) &u.fmt.ip, sizeof(u.fmt.ip));
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
                    /* ask the RDV server for a new connection with peer */
                    init_smsg(smsg, ASK_CONNECTION, dest.s_addr, 0);
                    sendto(sockfd,smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                    
                    clock_gettime(CLOCK_REALTIME, &timeout_connect);
                    timeout_connect.tv_sec += 3; // wait 3 secs
                    MUTEXLOCK;
                    if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                        peer = get_client_VPN(&dest);
                        if (peer != NULL) // the new connection is opened
                            SSL_write(peer->ssl, &u, r);
                    }
                    MUTEXUNLOCK;
                    
                }
                else switch (peer->state) {
                    /* Already connected */
                    case ESTABLISHED :
                        SSL_write(peer->ssl, &u, r);
                        break;
                    /* Lost connection */
                    case TIMEOUT :
                        /* try to reopen the connection */
                        MUTEXLOCK;
                        peer->time = time(NULL);
                        MUTEXUNLOCK;
                        /* ask the RDV server for a new connection with peer */
                        init_smsg(smsg, ASK_CONNECTION, dest.s_addr, 0);
                        sendto(sockfd,smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                        
                        clock_gettime(CLOCK_REALTIME, &timeout_connect);
                        timeout_connect.tv_sec += 3;
                        MUTEXLOCK;
                        if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                            peer = get_client_VPN(&dest);
                            if (peer != NULL) // the new connection is opened
                                SSL_write(peer->ssl, &u, r);
                        }
                        MUTEXUNLOCK;
                        break;
                    /* TODO: clean old unused states */
                    /* the client is not connected, but we have a structure */
                    case NOT_CONNECTED :
                        if (time(NULL)-peer->time>config.timeout) {
                            MUTEXLOCK;
                            // give the peer another chance
                            peer->state = TIMEOUT;
                            MUTEXUNLOCK;
                        }
                        break;
                    /* PUNCHING or WAITING
                     * The connection is in progress
                     */
                    default :
                        clock_gettime(CLOCK_REALTIME, &timeout_connect);
                        timeout_connect.tv_sec += 2; // wait another 2 secs
                        MUTEXLOCK;
                        if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                            peer = get_client_VPN(&dest);
                            if (peer != NULL) // the new connection is opened
                                SSL_write(peer->ssl, &u, r);
                        }
                        MUTEXUNLOCK;
                        break;
                }
            }
        }
    
    }
    
    free(smsg);
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
    args->sockfd = sockfd;
    args->tunfd = tunfd;
    
    SSL_library_init();
    SSL_load_error_strings();
    
    pthread_t th_socket, th_tun;
    th_socket = createThread(comm_socket, args);
    th_tun = createThread(comm_tun, args);
    
    pthread_join(th_socket, NULL);
    pthread_join(th_tun, NULL);
    
    free(args);
    
    struct client *peer, *next;
    peer = clients;
    while (peer != NULL) {
        next = peer->next;
        remove_client(peer); // wait for the end of the SSL_reading thread and free the structure
        peer = next;
    }
    
    
    init_smsg(&smsg, BYE, config.vpnIP.s_addr, 0);
    if (config.debug) printf("Sending BYE\n");
    if (sendto(sockfd,&smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr)) == -1) {
        perror("sendto");
    }
}


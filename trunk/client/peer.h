/*
 * Peers list management
 *
 * Copyright (C) 2008-2011 Florent Bondoux
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

#ifndef PEER_H_
#define PEER_H_

#include "pthread.h"
#include "rate_limiter.h"

/* clients states */
enum client_type {NEW, PUNCHING, LINKED, ESTABLISHED, CLOSED};

/* default receive timeout */
#define PEER_RECV_TIMEMOUT_USEC 250000
#define PEER_RECV_TIMEOUT_SEC 0

/* client storage structure */
struct client {
    time_t time;                    // last message received (time(NULL))
    time_t last_keepalive;          // last time we send a keepalive or a DTLS msg
    struct sockaddr_in clientaddr;  // real IP address and port
    struct in_addr vpnIP;           // VPN IP address
    int state;                      // client's state
    int tunfd;                      // tun device file descriptor
    int sockfd;                     // local UDP socket file descriptor
    struct client *next;            // next client in list
    struct client *prev;            // previous client in list
    pthread_cond_t cond_connected;  // pthread_cond used during connection
    SSL *ssl;                       // SSL structure
    BIO *wbio;                      // BIO (I/O abstraction) used for outgoing packets
    BIO *rbio;                      // BIO for incoming packets
    BIO *out_fifo;                  // FIFO BIO for outgoing packets
    SSL_CTX *ctx;                   // SSL context associated to the connection
    int is_dtls_client;             // DTLS client or server ?
    int shutdown;                   // Set to 1 by end_peer_handling
    int rdv_answer;                 // The answer from the RDV (ANS_CONNECTION or REJ_CONNECTION)
    struct tb_state rate_limiter;   // Rate limiter for this client

    pthread_mutex_t mutex;          // local mutex;

    pthread_mutex_t mutex_ref;      // mutex used to change the reference counter
    unsigned int ref_count;         // reference counter
};



extern struct client *peers_list;
extern int peers_n_clients;
extern pthread_mutex_t peers_mutex;

/* mutex manipulation */
#define GLOBAL_MUTEXLOCK {/*fprintf(stderr, "lock %s %d\n",__FILE__,__LINE__);*/mutexLock(&peers_mutex);}
#define GLOBAL_MUTEXUNLOCK {/*fprintf(stderr, "unlock %s %d\n",__FILE__,__LINE__);*/mutexUnlock(&peers_mutex);}
#define CLIENT_MUTEXLOCK(c) {/*fprintf(stderr, "lock %d %s %d\n",(c)->vpnIP.s_addr,__FILE__,__LINE__);*/mutexLock(&(c)->mutex);}
#define CLIENT_MUTEXUNLOCK(c) {/*fprintf(stderr, "unlock %d %s %d\n",(c)->vpnIP.s_addr,__FILE__,__LINE__);*/mutexUnlock(&(c)->mutex);}

extern void peers_mutex_init(void);
extern void peers_mutex_destroy(void);

extern struct client * peers_add_requested(int sockfd, int tunfd, int state,
        time_t t, struct in_addr vpnIP);
extern struct client * peers_add_caller(int sockfd, int tunfd, int state,
        time_t t, struct in_addr clientIP, uint16_t clientPort,
        struct in_addr vpnIP);
extern int peers_register_endpoint(struct client *peer);
extern void peers_remove(struct client *peer);

extern struct client * peers_get_by_VPN(struct in_addr *address);
extern struct client * peers_get_by_endpoint(struct sockaddr_in *cl_address);

extern void peers_incr_ref(struct client *peer);
extern void peers_decr_ref(struct client *peer, int n);

/* update the activity timestamp and the link activity timestamp */
#define peers_update_peer_time(peer,timestamp) ({\
    time_t t = timestamp;\
    struct client * c = peer;\
    c->time = t;\
    c->last_keepalive = t;\
    })

#endif /*PEER_H_*/

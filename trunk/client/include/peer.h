/*
 * Peers list management
 *
 * Copyright (C) 2008 Florent Bondoux
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

/* clients states */
enum client_type {PUNCHING, WAITING, ESTABLISHED, TIMEOUT, CLOSED};

/* client storage structure */
struct client {
    time_t time;                    // last message received (time(NULL))
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
    SSL_CTX *ctx;                   // SSL context associated to the connection
    int is_dtls_client;             // DTLS client or server ?
    int thread_ssl_running;         // the thread from SSL_reading is running
    int send_shutdown;              // Send a shutdown message after SSL_read returns 0

    pthread_mutex_t mutex_ref;      // mutex used to change the reference counter
    unsigned int ref_count;         // reference counter
};



extern struct client *clients;
extern int n_clients;
extern pthread_mutex_t mutex_clients;

/* mutex manipulation */
#define MUTEXLOCK {/*fprintf(stderr, "lock %s %d\n",__FILE__,__LINE__);*/mutexLock(&mutex_clients);}
#define MUTEXUNLOCK {/*fprintf(stderr, "unlock %s %d\n",__FILE__,__LINE__);*/mutexUnlock(&mutex_clients);}

extern void mutex_clients_init(void);
extern void mutex_clients_destroy(void);

extern struct client * add_client(int sockfd, int tunfd, int state, time_t time, struct in_addr clientIP, uint16_t clientPort, struct in_addr vpnIP, int is_dtls_client);
extern int createClientSSL(struct client *peer, int recreate);
extern void remove_client(struct client *peer);

extern struct client * _get_client_VPN(struct in_addr *address);
extern struct client * _get_client_real(struct sockaddr_in *cl_address);
extern struct client *cache_client_VPN;
extern struct client *cache_client_real;

extern void incr_ref(struct client *peer);
extern void decr_ref(struct client *peer);

/* get_client_VPN and get_client_real check the cache before calling _get_client_* */
#define get_client_VPN(__address) ({    \
    struct in_addr *address = __address;    \
    (cache_client_VPN != NULL    \
        && cache_client_VPN->vpnIP.s_addr == (address)->s_addr) ? \
                incr_ref(cache_client_VPN), cache_client_VPN : _get_client_VPN((address));    \
    })

#define get_client_real(__address) ({   \
    struct sockaddr_in *address = __address;    \
    (cache_client_real != NULL   \
        && cache_client_real->clientaddr.sin_addr.s_addr == (address)->sin_addr.s_addr    \
        && cache_client_real->clientaddr.sin_port == (address)->sin_port) ?   \
                incr_ref(cache_client_real), cache_client_real : _get_client_real((address)); \
    })

#endif /*PEER_H_*/

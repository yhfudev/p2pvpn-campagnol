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
#define PUNCHING 0
#define WAITING 1
#define ESTABLISHED 2
#define UNKNOWN 3
#define TIMEOUT 4
#define NOT_CONNECTED 5

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
    int thread_running;             // the thread from SSL_reading is running
    pthread_t thread;               // SSL_reading thread
};



extern struct client *clients;
extern pthread_mutex_t mutex_clients;

/* mutex manipulation */
#define MUTEXLOCK {mutexLock(&mutex_clients);}
#define MUTEXUNLOCK {mutexUnlock(&mutex_clients);}

extern struct client * add_client(int sockfd, int tunfd, int state, time_t time, struct in_addr clientIP, u_int16_t clientPort, struct in_addr vpnIP, int is_dtls_client);
extern void createClientSSL(struct client *peer, int recreate);
extern void remove_client(struct client *peer);
extern struct client * get_client_VPN(struct in_addr *address);
extern struct client * get_client_real(struct sockaddr_in *cl_address);

#endif /*PEER_H_*/

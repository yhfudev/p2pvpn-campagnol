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

// etats des clients
#define PUNCHING 0
#define WAITING 1
#define ESTABLISHED 2
#define UNKNOWN 3
#define TIMEOUT 4
#define NOT_CONNECTED 5

/** structure d'un client */
struct client {
    time_t time;                    // dernier message recu (time(NULL))
    struct sockaddr_in clientaddr;  // addresse réelle du client
    struct in_addr vpnIP;           // vpnIP du client
    int state;                      // état du client
    int tunfd;                      // fd du périphérique tun
    int sockfd;
    struct client *next;            // client suivant dans la liste
    struct client *prev;            // client précédant dans la liste
    pthread_cond_t cond_connected;  // condition pour indiquer que le client est connecté
    SSL *ssl;                       // connexion SSL
    BIO *wbio;                      // BIO (abstraction I/O) utilisé en écriture par SSL
    BIO *rbio;                      // BIO utilisé en lecture par SSL
    SSL_CTX *ctx;                   // contexte SSL associé à la connexion
    int is_dtls_client;             // client ou serveur DTLS
    int thread_running;             // le thread de lecture SSL est lancé
    pthread_t thread;               // le thread
};
/** fin structure */



extern struct client *clients;
extern pthread_mutex_t mutex_clients;
// Manipulation du mutex
#define MUTEXLOCK {mutexLock(&mutex_clients);}
#define MUTEXUNLOCK {mutexUnlock(&mutex_clients);}

extern struct client * add_client(int sockfd, int tunfd, int state, time_t time, struct in_addr clientIP, u_int16_t clientPort, struct in_addr vpnIP, int is_dtls_client);
extern void createClientSSL(struct client *peer, int recreate);
extern void remove_client(struct client *peer);
extern struct client * get_client_VPN(struct in_addr *address);
extern struct client * get_client_real(struct sockaddr_in *cl_address);

#endif /*PEER_H_*/

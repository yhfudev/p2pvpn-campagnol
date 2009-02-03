/*
 * Peers list management
 *
 * Copyright (C) 2008-2009 Florent Bondoux
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

/*
 * Definition of the list of known clients and manipulation functions:
 * add/remove, search by IP/port or by VPN IP
 */
#include "campagnol.h"
#include "peer.h"
#include "dtls_utils.h"
#include "pthread_wrap.h"
#include "../common/log.h"

#include <arpa/inet.h>

/* List of known clients */
struct client *clients = NULL;
int n_clients = 0;

/*
 * mutex used to manipulate 'clients'
 * It needs to be recursive since decr_ref may lock it
 */
pthread_mutex_t mutex_clients;
pthread_mutexattr_t attrs_mutex_clients;

void mutex_clients_init(void) {
    mutexattrInit(&attrs_mutex_clients);
    mutexattrSettype(&attrs_mutex_clients, PTHREAD_MUTEX_RECURSIVE);
    mutexInit(&mutex_clients, &attrs_mutex_clients);
}

void mutex_clients_destroy(void) {
    mutexDestroy(&mutex_clients);
    mutexattrDestroy(&attrs_mutex_clients);
}

/*
 * Safely increment the reference counter of peer
 */
void incr_ref(struct client *peer) {
    mutexLock(&peer->mutex_ref);
    peer->ref_count++;
//    fprintf(stderr, "incr %s [%d]\n", inet_ntoa(peer->vpnIP), peer->ref_count);
    mutexUnlock(&peer->mutex_ref);
}

/*
 * Safely decrement the reference counter of peer
 * If the count reaches 0, the peer is freed
 */
void decr_ref(struct client *peer) {
    mutexLock(&peer->mutex_ref);
    peer->ref_count--;
//    fprintf(stderr, "decr %s [%d]\n", inet_ntoa(peer->vpnIP), peer->ref_count);
    if (peer->ref_count == 0) {
        mutexUnlock(&peer->mutex_ref);
        MUTEXLOCK;
        remove_client(peer);
        MUTEXUNLOCK;
    }
    else {
        mutexUnlock(&peer->mutex_ref);
    }
}

/*
 * Add a client to the list
 *
 * !! The reference counter of the new client is 2:
 * one reference in the linked list "clients" plus the returned reference
 *
 * just call decr_ref to remove the last reference from "clients":
 * This will remove the client from the linked list and free it's memory
 */
struct client * add_client(int sockfd, int tunfd, int state, time_t time, struct in_addr clientIP, uint16_t clientPort, struct in_addr vpnIP, int is_dtls_client) {
    int r;

    if (n_clients >= config.max_clients) {
        if (config.debug) {
            printf("Cannot open a new connection: maximum number of connections reached\n");
        }
        return NULL;
    }

    if (config.debug) printf("Adding new client %s\n", inet_ntoa(vpnIP));
    struct client *peer = malloc(sizeof(struct client));
    if (peer == NULL) {
        log_error("Cannot allocate a new client (malloc)");
        return NULL;
    }
    peer->time = time;
    peer->last_keepalive = time;
    memset(&(peer->clientaddr), 0, sizeof(peer->clientaddr));
    peer->clientaddr.sin_family = AF_INET;
    peer->clientaddr.sin_addr = clientIP;
    peer->clientaddr.sin_port = clientPort;
    peer->vpnIP = vpnIP;
    peer->state = state;
    peer->tunfd = tunfd;
    peer->sockfd = sockfd;
    conditionInit(&peer->cond_connected, NULL);
    peer->send_shutdown = 0;

    /* initialize rate limiter */
    if (config.tb_connection_size != 0) {
        tb_init(&peer->rate_limiter, config.tb_connection_size, (double) config.tb_connection_rate, 8);
    }

    peer->is_dtls_client = is_dtls_client;
    peer->thread_ssl_running = 0;
    mutexInit(&(peer->mutex_ref), NULL);
    peer->ref_count = 2;

    r = createClientSSL(peer);
    if (r != 0) {
        free(peer);
        log_error("Could not create the new client");
        return NULL;
    }

    peer->next = clients;
    peer->prev = NULL;
    if (peer->next) peer->next->prev = peer;
    clients = peer;
    n_clients ++;
    return peer;
}

/*
 * Contains the last search results for
 * _get_client_VPN and _get_client_real
 */
struct client *cache_client_VPN = NULL;
struct client *cache_client_real = NULL;

/*
 * Remove a client from the list "clients"
 * Free the associated SSL structures
 */
void remove_client(struct client *peer) {
    if (config.debug) printf("Deleting the client %s\n", inet_ntoa(peer->vpnIP));

    conditionDestroy(&peer->cond_connected);
    mutexDestroy(&peer->mutex_ref);
    SSL_free(peer->ssl);

    if (peer->next) peer->next->prev = peer->prev;
    if (peer->prev) {
        peer->prev->next = peer->next;
    }
    else {
        clients = peer->next;
    }

    free(peer);
    cache_client_VPN = NULL;
    cache_client_real = NULL;
    n_clients --;
}

/*
 * Get a client by its VPN IP address and increments its ref. counter
 * return NULL if the client is unknown
 */
struct client * _get_client_VPN(struct in_addr *address) {
    struct client *peer = clients;
    while (peer != NULL) {
        if (peer->vpnIP.s_addr == address->s_addr) {
            /* found */
            cache_client_VPN = peer;
            incr_ref(peer);
            return peer;
        }
        peer = peer->next;
    }
    return NULL;
}

/*
 * Get a client by its real IP address and UDP port and increments its ref. counter
 * return NULL if the client is unknown
 */
struct client * _get_client_real(struct sockaddr_in *cl_address) {
    struct client *peer = clients;
    while (peer != NULL) {
        if (peer->clientaddr.sin_addr.s_addr == cl_address->sin_addr.s_addr
            && peer->clientaddr.sin_port == cl_address->sin_port) {
            /* found */
            cache_client_real = peer;
            incr_ref(peer);
            return peer;
        }
        peer = peer->next;
    }
    return NULL;
}
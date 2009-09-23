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
#include "../common/pthread_wrap.h"
#include "../common/log.h"

#include <arpa/inet.h>
#include <search.h>

/* List of known clients */
struct client *clients = NULL;
int n_clients = 0;
/* tree, items ordered by VPN IP */
static void *clients_vpn_root = NULL;
/* tree, items ordered by public endpoint */
static void *clients_address_root = NULL;

/* comparison routine for clients_vpn_root (by VPN IP) */
static int compare_clients_vpn(const void *itema, const void *itemb) {
    const struct client *peer1 = (const struct client *) itema;
    const struct client *peer2 = (const struct client *) itemb;
    return memcmp(&peer1->vpnIP, &peer2->vpnIP, sizeof(peer1->vpnIP));
}

/* comparison routine for clients_address_root (by public endpoint) */
static int compare_clients_addr(const void *itema, const void *itemb) {
    const struct client *peer1 = (const struct client *) itema;
    const struct client *peer2 = (const struct client *) itemb;
    if (peer1->clientaddr.sin_addr.s_addr < peer2->clientaddr.sin_addr.s_addr)
        return -1;
    if (peer1->clientaddr.sin_addr.s_addr == peer2->clientaddr.sin_addr.s_addr) {
        if (peer1->clientaddr.sin_port < peer2->clientaddr.sin_port)
            return -1;
        if (peer1->clientaddr.sin_port == peer2->clientaddr.sin_port) {
            return 0;
        }
        return 1;
    }
    return 1;
}

/*
 * mutex used to manipulate the clients list and the trees
 */
pthread_mutex_t mutex_clients;

void mutex_clients_init(void) {
    pthread_mutexattr_t attrs;
    mutexattrInit(&attrs);
    mutexattrSettype(&attrs, PTHREAD_MUTEX_RECURSIVE);
    mutexInit(&mutex_clients, &attrs);
}

void mutex_clients_destroy(void) {
    mutexDestroy(&mutex_clients);
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
void decr_ref(struct client *peer, int n) {
    GLOBAL_MUTEXLOCK;
    mutexLock(&peer->mutex_ref);
    peer->ref_count -= n;
//    fprintf(stderr, "decr %s [%d]\n", inet_ntoa(peer->vpnIP), peer->ref_count);
    if (peer->ref_count == 0) {
        mutexUnlock(&peer->mutex_ref);
        remove_client(peer);
    }
    else {
        mutexUnlock(&peer->mutex_ref);
    }
    GLOBAL_MUTEXUNLOCK;
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
struct client * add_client(int sockfd, int tunfd, int state, time_t t, struct in_addr clientIP, uint16_t clientPort, struct in_addr vpnIP, int is_dtls_client) {
    int r;
    void *slot;

    GLOBAL_MUTEXLOCK;

    if (n_clients >= config.max_clients) {
        if (config.debug) {
            printf("Cannot open a new connection: maximum number of connections reached\n");
        }
        GLOBAL_MUTEXUNLOCK;
        return NULL;
    }

    if (config.debug) printf("Adding new client %s\n", inet_ntoa(vpnIP));
    struct client *peer = malloc(sizeof(struct client));
    if (peer == NULL) {
        log_error(errno, "Cannot allocate a new client (malloc)");
        GLOBAL_MUTEXUNLOCK;
        return NULL;
    }
    peer->time = t;
    peer->last_keepalive = t;
    memset(&(peer->clientaddr), 0, sizeof(peer->clientaddr));
    peer->clientaddr.sin_family = AF_INET;
    peer->clientaddr.sin_addr = clientIP;
    peer->clientaddr.sin_port = clientPort;
    peer->vpnIP = vpnIP;
    peer->state = state;
    peer->tunfd = tunfd;
    peer->sockfd = sockfd;
    conditionInit(&peer->cond_connected, NULL);
    mutexInit(&peer->mutex, NULL);
    peer->shutdown = 0;
    peer->is_dtls_client = is_dtls_client;
    mutexInit(&(peer->mutex_ref), NULL);
    peer->ref_count = 2;

    /* initialize rate limiter */
    if (config.tb_connection_size != 0) {
        tb_init(&peer->rate_limiter, config.tb_connection_size, (double) config.tb_connection_rate, 8, 1);
    }


    r = createClientSSL(peer);
    if (r != 0) {
        mutexDestroy(&peer->mutex_ref);
        mutexDestroy(&peer->mutex);
        conditionDestroy(&peer->cond_connected);
        free(peer);
        log_error(-1, "Could not create the new client");
        GLOBAL_MUTEXUNLOCK;
        return NULL;
    }

    slot = tsearch((void *) peer, &clients_vpn_root, compare_clients_vpn);
    if (slot == NULL) {
        log_error(errno, "Cannot allocate a new client (tsearch)");
        mutexDestroy(&peer->mutex_ref);
        mutexDestroy(&peer->mutex);
        conditionDestroy(&peer->cond_connected);
        SSL_free(peer->ssl);
        free(peer);
        GLOBAL_MUTEXUNLOCK;
        return NULL;
    }

    /*
     * Only add the client to clients_address_root if the real endpoint is
     * available. Otherwise, need to call register_client_endpoint later.
     */
    if (clientPort != 0) {
        slot = tsearch((void *) peer, &clients_address_root, compare_clients_addr);
        if (slot == NULL) {
            log_error(errno, "Cannot allocate a new client (tsearch)");
            tdelete(peer, &clients_address_root, compare_clients_addr);
            mutexDestroy(&peer->mutex_ref);
            mutexDestroy(&peer->mutex);
            conditionDestroy(&peer->cond_connected);
            SSL_free(peer->ssl);
            free(peer);
            GLOBAL_MUTEXUNLOCK;
            return NULL;
        }
    }

    peer->next = clients;
    peer->prev = NULL;
    if (peer->next) peer->next->prev = peer;
    clients = peer;
    n_clients ++;
    GLOBAL_MUTEXUNLOCK;
    return peer;
}

/*
 * Add the client to the real address tree.
 */
int register_client_endpoint(struct client *peer) {
    void *slot;
    GLOBAL_MUTEXLOCK;
    slot = tsearch((void *) peer, &clients_address_root, compare_clients_addr);
    if (slot == NULL) {
        log_error(errno, "Cannot allocate a new client (tsearch)");
        GLOBAL_MUTEXUNLOCK;
        return -1;
    }
    GLOBAL_MUTEXUNLOCK;
    return 0;
}

/*
 * Remove a client from the list "clients"
 * Free the associated SSL structures
 */
void remove_client(struct client *peer) {
    GLOBAL_MUTEXLOCK;
    if (config.debug) printf("Deleting the client %s\n", inet_ntoa(peer->vpnIP));

    conditionDestroy(&peer->cond_connected);
    mutexDestroy(&peer->mutex);
    mutexDestroy(&peer->mutex_ref);
    /* clean rate limiter */
    if (config.tb_connection_size != 0) {
        tb_clean(&peer->rate_limiter);
    }

    SSL_free(peer->ssl);
    BIO_free(peer->out_fifo);

    if (peer->next) peer->next->prev = peer->prev;
    if (peer->prev) {
        peer->prev->next = peer->next;
    }
    else {
        clients = peer->next;
    }

    tdelete(peer, &clients_vpn_root, compare_clients_vpn);
    tdelete(peer, &clients_address_root, compare_clients_addr);

    free(peer);
    n_clients --;
    GLOBAL_MUTEXUNLOCK;
}

/*
 * Get a client by its VPN IP address and increments its ref. counter
 * return NULL if the client is unknown
 * the client's mutex is locked.
 */
struct client * get_client_VPN(struct in_addr *address) {
    struct client peer_tmp, *peer;
    GLOBAL_MUTEXLOCK;
    peer_tmp.vpnIP.s_addr = address->s_addr;
    peer = tfind(&peer_tmp, &clients_vpn_root, compare_clients_vpn);
    if (peer != NULL) {
        peer = *(void **) peer;
        incr_ref(peer);
        CLIENT_MUTEXLOCK(peer);
    }
    GLOBAL_MUTEXUNLOCK;
    return peer;
}

/*
 * Get a client by its real IP address and UDP port and increments its ref. counter
 * return NULL if the client is unknown
 * the client's mutex is locked.
 */
struct client * get_client_real(struct sockaddr_in *cl_address) {
    struct client peer_tmp, *peer;
    GLOBAL_MUTEXLOCK;
    peer_tmp.clientaddr.sin_addr.s_addr = cl_address->sin_addr.s_addr;
    peer_tmp.clientaddr.sin_port = cl_address->sin_port;
    peer = tfind(&peer_tmp, &clients_address_root, compare_clients_addr);
    if (peer != NULL) {
        peer = *(void **)peer;
        incr_ref(peer);
        CLIENT_MUTEXLOCK(peer);
    }
    GLOBAL_MUTEXUNLOCK;
    return peer;
}

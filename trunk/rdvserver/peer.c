/*
 * Peer list definition
 *
 * Copyright (C) 2009 Florent Bondoux
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
#include "rdv.h"
#include "peer.h"
#include "../common/log.h"

#include <arpa/inet.h>
#include <search.h>

/* List of known clients */
struct client *clients = NULL;
int n_clients = 0;

/* tree, items ordered by real address */
static void *clients_address_root = NULL;
/* tree, items ordered by VPN IP */
static void *clients_vpn_root = NULL;

/* comparison routine for the first tree */
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

/* comparison routine for clients_vpn_root */
static int compare_clients_vpn(const void *itema, const void *itemb) {
    const struct client *peer1 = (const struct client *) itema;
    const struct client *peer2 = (const struct client *) itemb;
    if (peer1->vpnIP.s_addr < peer2->vpnIP.s_addr)
        return -1;
    if (peer1->vpnIP.s_addr == peer2->vpnIP.s_addr)
        return 0;
    return 1;
}

/*
 * Add a client to the list/trees
 */
struct client * add_client(int sockfd, time_t t, struct in_addr clientIP,
        uint16_t clientPort, struct in_addr vpnIP, struct in_addr localIP,
        uint16_t localPort) {
    int r;
    void *slot1, *slot2;

    if (config.max_clients != 0 && n_clients >= config.max_clients) {
        log_message_level(2, "Cannot register a new client: maximum number of clients reached");
        return NULL;
    }

    struct client *peer = malloc(sizeof(struct client));
    if (peer == NULL) {
        log_error(errno, "Cannot allocate a new client (malloc)");
        return NULL;
    }
    peer->time = t;
    memset(&(peer->clientaddr), 0, sizeof(peer->clientaddr));
    memset(&(peer->localaddr), 0, sizeof(peer->localaddr));
    peer->clientaddr.sin_family = AF_INET;
    peer->clientaddr.sin_addr = clientIP;
    peer->clientaddr.sin_port = clientPort;
    peer->localaddr.sin_family = AF_INET;
    peer->localaddr.sin_addr = localIP;
    peer->localaddr.sin_port = localPort;
    peer->vpnIP = vpnIP;
    peer->sockfd = sockfd;

    slot1 = tsearch((void *) peer, &clients_address_root, compare_clients_addr);
    if (slot1 == NULL) {
        log_error(errno, "Cannot allocate a new client (tsearch)");
        free(peer);
        return NULL;
    }
    slot2 = tsearch((void *) peer, &clients_vpn_root, compare_clients_vpn);
    if (slot2 == NULL) {
        log_error(errno, "Cannot allocate a new client (tsearch)");
        tdelete(peer, &clients_address_root, compare_clients_addr);
        free(peer);
        return NULL;
    }

    peer->vpnIP_string = CHECK_ALLOC_FATAL(strdup(inet_ntoa(vpnIP)));
    peer->clientaddr_string = CHECK_ALLOC_FATAL(malloc(32));
    if (peer->clientaddr_string != NULL) {
        r = snprintf(peer->clientaddr_string, 32, "%s/%d", inet_ntoa(clientIP), ntohs(clientPort));
        if (r >= 32) {
            peer->clientaddr_string[31] = '\0';
        }
    }

    log_message_level(1, "Adding new client [%s] %s", peer->clientaddr_string,
            peer->vpnIP_string);

    peer->next = clients;
    peer->prev = NULL;
    if (peer->next)
        peer->next->prev = peer;
    clients = peer;

    n_clients++;
    return peer;
}

/*
 * Remove a client from the list and trees
 */
void remove_client(struct client *peer) {
    log_message_level(1, "Deleting the client [%s] %s", peer->clientaddr_string,
            peer->vpnIP_string);

    if (peer->next)
        peer->next->prev = peer->prev;
    if (peer->prev) {
        peer->prev->next = peer->next;
    }
    else {
        clients = peer->next;
    }

    free(peer->clientaddr_string);
    free(peer->vpnIP_string);

    tdelete(peer, &clients_address_root, compare_clients_addr);
    tdelete(peer, &clients_vpn_root, compare_clients_vpn);

    free(peer);
    n_clients--;
}

/*
 * Get a client by its VPN IP address
 * return NULL if the client is unknown
 */
struct client * get_client_VPN(struct in_addr *address) {
    struct client peer_tmp, *peer;
    peer_tmp.vpnIP.s_addr = address->s_addr;
    peer = tfind(&peer_tmp, &clients_vpn_root, compare_clients_vpn);
    if (peer != NULL)
        peer = *(void **) peer;
    return peer;
}

/*
 * Get a client by its real IP address and UDP port
 * return NULL if the client is unknown
 */
struct client * get_client_real(struct sockaddr_in *cl_address) {
    struct client peer_tmp, *peer;
    peer_tmp.clientaddr.sin_addr.s_addr = cl_address->sin_addr.s_addr;
    peer_tmp.clientaddr.sin_port = cl_address->sin_port;
    peer = tfind(&peer_tmp, &clients_address_root, compare_clients_addr);
    if (peer != NULL)
        peer = *(void **) peer;
    return peer;
}

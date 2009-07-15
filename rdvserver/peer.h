/*
 * Peers list management
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

#ifndef PEER_H_
#define PEER_H_

#include <time.h>

/* clients states */
enum client_type {
    PUNCHING, WAITING, ESTABLISHED, TIMEOUT, CLOSED
};

#define PEER_TIMEOUT 5

/* client storage structure */
struct client {
    time_t time;                        // last message received (time(NULL))
    struct sockaddr_in clientaddr;      // real IP address and port
    struct sockaddr_in localaddr;       // client's local address
    struct in_addr vpnIP;               // VPN IP address
    int sockfd;                         // local UDP socket file descriptor
    char * vpnIP_string;                // string representation of the VPN IP
    char * clientaddr_string;           // string representation of the client address
    struct client *next;                // next client in list
    struct client *prev;                // previous client in list
};

extern struct client *clients;
extern int n_clients;

extern struct client * add_client(int sockfd, time_t t,
        struct in_addr clientIP, uint16_t clientPort, struct in_addr vpnIP,
        struct in_addr localIP, uint16_t localPort);
extern void remove_client(struct client *peer);

extern struct client * get_client_VPN(struct in_addr *address);
extern struct client * get_client_real(struct sockaddr_in *cl_address);

/* update the activity timestamp and the link activity timestamp */
#define client_update_time(peer) ({\
    (peer)->time = time(NULL);\
    })

#define client_is_timeout(peer) ((time(NULL) - (peer)->time) > PEER_TIMEOUT)
#define client_is_dead(peer) ((time(NULL) - (peer)->time) > (2 * PEER_TIMEOUT))

#endif /*PEER_H_*/

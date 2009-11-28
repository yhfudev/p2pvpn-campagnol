/*
 * UDP socket management
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

#include <net/if.h>
#include <arpa/inet.h>

#include "configuration.h"
#include "net_socket.h"
#include "../common/log.h"

/* Create the UDP socket
 * Bind it to config.localIP
 *            config.localport (localport > 0)
 *            config.iface (iface != NULL)
 */
int create_socket(void) {
    int sockfd;
    struct sockaddr_in localaddr, tmp_addr;
    socklen_t tmp_addr_len;

    /* Socket creation */
    log_message_level(2, "Creating the UDP socket...");
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd<0) {
        log_error(errno, "Could not create the socket");
        return -1;
    }

#ifdef HAVE_LINUX
    if (config.iface != NULL) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        if (strlen(config.iface) + 1 > IFNAMSIZ) {
            log_message("The interface name '%s' is too long", config.iface);
            return -1;
        }
        strcpy(ifr.ifr_name, config.iface);
        if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) {
            log_error(errno, "Could not bind the socket to the interface (%s)", config.iface);
            return -1;
        }
    }
#endif

    memset(&localaddr, 0, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr=config.localIP.s_addr;
    if (config.localport != 0) localaddr.sin_port=htons(config.localport);
    if (bind(sockfd,(struct sockaddr *)&localaddr,sizeof(localaddr))<0) {
        log_error(errno,
                "Could not bind the socket to the local IP address (%s port %u)",
                inet_ntoa(config.localIP), config.localport);
        return -1;
    }
    log_message_level(1, "Socket opened");

    /* Get the local port */
    if (config.localport == 0) {
        tmp_addr_len = sizeof(tmp_addr);
        getsockname(sockfd, (struct sockaddr *) &tmp_addr, &tmp_addr_len);
        config.localport = ntohs(tmp_addr.sin_port);
    }

    return sockfd;
}


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

#include <net/if.h>
#include <arpa/inet.h>

#include "campagnol.h"
#include "communication.h"
#include "net_socket.h"
#include "log.h"

/* Create the UDP socket
 * Bind it to config.localIP
 *            config.localport (localport > 0)
 *            config.iface (strlen(iface) > 0)
 */
int create_socket(void) {
    int sockfd;
    struct sockaddr_in localaddr;
    
    /* Socket creation */
    if (config.debug) printf("Create the UDP socket...\n");
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd<0) {
        log_message("Error: creating socket");
        return -1;
    }
    
    if (strlen(config.iface) != 0) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_ifrn.ifrn_name, config.iface, IFNAMSIZ);
        if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) {
            log_error("Error: binding socket to interface");
            log_message("interface: %s", config.iface);
            return -1;
        }
    }
    
    bzero(&localaddr, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr=config.localIP.s_addr;
    if (config.localport != 0) localaddr.sin_port=htons(config.localport);
    if (bind(sockfd,(struct sockaddr *)&localaddr,sizeof(localaddr))<0) {
        log_error("Error: binding socket to local IP address");
        log_message("address: %s", inet_ntoa(config.localIP));
        return -1;
    }
    if (config.verbose) printf("Socket opened\n");
    
    return sockfd;
}


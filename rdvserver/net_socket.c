/*
 * UDP socket management
 *
 * Copyright (C) 2007 Antoine Vianey
 *               2008-2009 Florent Bondoux
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
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * 
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 *
 */

#include "rdv.h"

#include <net/if.h>
#include <arpa/inet.h>

#include "net_socket.h"
#include "../common/log.h"

/* Create the UDP socket
 * Bind it to config.serverport (serverport > 0)
 */
int create_socket(void) {
    int sockfd;
    struct sockaddr_in localaddr;

    /* Socket creation */
    log_message_level(2, "Creating the UDP socket...");
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd<0) {
        log_error(errno, "Could not create the socket");
        return -1;
    }

    memset(&localaddr, 0, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = INADDR_ANY;
    if (config.serverport != 0) localaddr.sin_port=htons(config.serverport);
    if (bind(sockfd,(struct sockaddr *)&localaddr,sizeof(localaddr))<0) {
        log_error(errno, "Could not bind the socket to the port (%d)",
                config.serverport);
        return -1;
    }

    return sockfd;
}

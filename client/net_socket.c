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

#include "campagnol.h"
#include "communication.h"
#include "net_socket.h"

/* Création socket liée à IP+port donnés, 
 * et éventuellement à l'interface donnée si strlen(iface) > 0
 */
int create_socket(struct in_addr *localIP, int localport, char *iface) {
    int sockfd;
    struct sockaddr_in localaddr;
    
    /** creation de la socket */
    if (config.verbose) printf("Création de la socket...\n");
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd<0) {
        if (config.verbose) printf("Erreur d'ouverture de socket\n");
        exit(1);
    }
    
    if (strlen(iface) != 0) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_ifrn.ifrn_name, iface, IFNAMSIZ);
        if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) {
            perror("Erreur en liant la socket à l'interface donnée");
            fprintf(stderr, "%s\n", iface);
            exit(1);
        }
    }
    
    bzero(&localaddr, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr=localIP->s_addr;
    if (localport != 0) localaddr.sin_port=htons(localport);
    if (bind(sockfd,(struct sockaddr *)&localaddr,sizeof(localaddr))<0) {
        perror("Erreur d'ouverture de socket");
        exit(1);
    }
    if (config.verbose) printf("Socket configuree\n");
    
    return sockfd;
}


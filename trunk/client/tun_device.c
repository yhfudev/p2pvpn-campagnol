/*
 * Create and configure a tun device
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include "campagnol.h"
#include "tun_device.h"

/* Ouverture d'une interface tun, liée à l'adresse donnée
 */
int init_tun(struct in_addr *vpnIP, int istun) {
    int tunfd;
    struct ifreq ifr;                           // pour l'ouverture du device tun
    char systemcall[100] = "";                  // utilisée pour configurer le driver tun via system
    
    /** Initialisation de l'interface TUN */
    if (config.verbose) printf("Initialisation de l'interface TUN\n");
    if( (tunfd = open("/dev/net/tun", O_RDWR)) < 0 ) {
         perror("Impossible d'ouvrir /dev/net/tun");
         exit(1);
    }

    bzero(&ifr, sizeof(ifr));
    
    /** Flags:  IFF_TUN   - TUN device (no Ethernet headers) 
                IFF_TAP   - TAP device  
                IFF_NO_PI - Do not provide packet information */ 
    ifr.ifr_flags = (istun ? IFF_TUN : IFF_TAP) |IFF_NO_PI ; 
    strncpy(ifr.ifr_name, (istun ? "tun%d" : "tap%d"), IFNAMSIZ);
    
    if ((ioctl(tunfd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(tunfd);
        perror("Erreur configuration du driver tun");
        exit(1);
    }
    if ((ioctl(tunfd, TUNSETNOCSUM, 1)) < 0) {
        close(tunfd);
        perror("Erreur configuration du driver tun");
        exit(1);
    }
    
    if (config.verbose) printf("le device utilise est : %s \n",ifr.ifr_name);
    
    /** Configuration de l'interface TUN */
    if (config.verbose) printf("Configuration de l'interface TUN\n");
    if (config.verbose) printf("ifconfig...\n");
    snprintf(systemcall, 100, "ifconfig %s %s mtu 1400 up", ifr.ifr_name, inet_ntoa (*vpnIP));
    if (config.verbose) puts(systemcall);
    system(systemcall);
    if (config.verbose) printf("ip route...\n");
    snprintf(systemcall, 100, "ip route replace %s via %s", config.network, inet_ntoa (*vpnIP));
    if (config.verbose) puts(systemcall);
    int r = system(systemcall);
    if (r != 0) { // iproute2 non installé sur la machine ?
        if (config.verbose) puts("route add...");
        snprintf(systemcall, 100, "route add -net %s gw %s", config.network, inet_ntoa (*vpnIP));
        if (config.verbose) puts(systemcall);
        r = system(systemcall);
    }
    if (r != 0) {
        fprintf(stderr, "Erreur configuration de l'interface TUN\n");
        exit(1);
    }
    /** L'interface TUN est configurée pour envoyer tous les messages à destination 
        du réseau NETMASK par l'addresse attribuée à l'interface TUN */
        
    return tunfd;
}

int close_tun(int fd) {
    return close(fd);
}


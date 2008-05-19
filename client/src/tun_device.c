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
#include <net/if.h>
#include <linux/if_tun.h>

#include "campagnol.h"
#include "tun_device.h"
#include "log.h"

/*
 * Open a new TUN virtual interface
 * Bind it to config.vpnIP
 * istun : use a TUN or TAP device
 */
int init_tun(int istun) {
    int tunfd;
    struct ifreq ifr;                   // interface request used to open the TUN device
    char systemcall[100] = "";          // used to configure the new interface with system shell commands
    
    /* Open TUN interface */
    if (config.verbose) printf("TUN interface initialization\n");
    if( (tunfd = open("/dev/net/tun", O_RDWR)) < 0 ) {
         log_error("Could not open /dev/net/tun");
         return -1;
    }

    bzero(&ifr, sizeof(ifr));
    
    /** Flags:  IFF_TUN   - TUN device (no Ethernet headers) 
                IFF_TAP   - TAP device  
                IFF_NO_PI - Do not provide packet information */ 
    ifr.ifr_flags = (istun ? IFF_TUN : IFF_TAP) |IFF_NO_PI ; 
    strncpy(ifr.ifr_name, (istun ? "tun%d" : "tap%d"), IFNAMSIZ);
    
    if ((ioctl(tunfd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(tunfd);
        log_error("Error: ioctl TUNSETIFF");
        return -1;
    }
    if ((ioctl(tunfd, TUNSETNOCSUM, 1)) < 0) {
        close(tunfd);
        log_error("Error: ioctl TUNSETNOCSUM");
        return -1;
    }
    
    if (config.debug) printf("Using TUN device: %s\n",ifr.ifr_name);
    
    /* Inteface configuration */
    if (config.verbose) printf("TUN interface configuration\n");
    if (config.debug) printf("ifconfig...\n");
    snprintf(systemcall, 100, "ifconfig %s %s mtu 1400 up", ifr.ifr_name, inet_ntoa (config.vpnIP));
    if (config.debug) puts(systemcall);
    system(systemcall);
    if (config.debug) printf("ip route...\n");
    snprintf(systemcall, 100, "ip route replace %s via %s", config.network, inet_ntoa (config.vpnIP));
    if (config.debug) puts(systemcall);
    int r = system(systemcall);
    if (r != 0) { // iproute2 is not installed ? try with route
        if (config.debug) puts("route add...");
        snprintf(systemcall, 100, "route add -net %s gw %s", config.network, inet_ntoa (config.vpnIP));
        if (config.debug) puts(systemcall);
        r = system(systemcall);
    }
    if (r != 0) {
        log_message_verb("Error while configuring the TUN interface");
        return -1;
    }
        
    return tunfd;
}

int close_tun(int fd) {
    return close(fd);
}


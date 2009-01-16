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

#include "campagnol.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_tun.h>
#include <sys/stat.h>

#include "configuration.h"
#include "communication.h"
#include "tun_device.h"
#include "../common/log.h"

/*
 * Open a new TUN virtual interface
 * Bind it to config.vpnIP
 * istun : use a TUN or TAP device
 */
int init_tun(int istun) {
    int tunfd;
    struct stat buf;
    int r;
    char systemcall[100] = "";          // used to configure the new interface with system shell commands

    /* Open TUN interface */
    if (config.verbose) printf("TUN interface initialization\n");
    if( (tunfd = open("/dev/tun", O_RDWR)) < 0 ) {
         log_error("Could not open /dev/net/tun");
         return -1;
    }

    int i=0;
    ioctl(tunfd, TUNSLMODE, &i);
    ioctl(tunfd, TUNSIFHEAD, &i);
    fstat(tunfd, &buf);

    /* Inteface configuration */
    if (config.verbose) printf("TUN interface configuration (%s MTU %d)\n", devname(buf.st_rdev, S_IFCHR), config.tun_mtu);
    if (config.debug) printf("ifconfig...\n");
    snprintf(systemcall, 100, "ifconfig %s inet %s %s mtu %d up", devname(buf.st_rdev, S_IFCHR), inet_ntoa (config.vpnIP), inet_ntoa(config.vpnIP), config.tun_mtu);
    if (config.debug) puts(systemcall);
    system(systemcall);
    if (config.debug) puts("route add...");
    snprintf(systemcall, 100, "route add -net %s %s", config.network, inet_ntoa (config.vpnIP));
    if (config.debug) puts(systemcall);
    r = system(systemcall);
    if (r != 0) {
        log_message_verb("Error while configuring the TUN interface");
        return -1;
    }

    return tunfd;
}

int close_tun(int fd) {
    struct stat buf;
    char systemcall[100] = "";

    fstat(fd, &buf);
    close(fd);
    snprintf(systemcall, 100, "ifconfig %s destroy", devname(buf.st_rdev, S_IFCHR));
    if (config.debug) puts(systemcall);
    system(systemcall);
    return 0;
}


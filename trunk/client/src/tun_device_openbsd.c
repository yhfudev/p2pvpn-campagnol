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
#include <net/if_types.h>
#include <net/if_tun.h>
#include <sys/uio.h>
#include <unistd.h>

#include "configuration.h"
#include "communication.h"
#include "tun_device.h"
#include "log.h"

/*
 * Open a new TUN virtual interface
 * Bind it to config.vpnIP
 * istun : use a TUN or TAP device
 */
int init_tun(int istun) {
    int tunfd;
    char systemcall[100] = "";          // used to configure the new interface with system shell commands
    int r, i;
    char devicename[20];
    struct tuninfo infos;

    /* Open TUN interface */
    if (config.verbose) printf("TUN interface initialization\n");
    for (i=0; i<255; i++) {
        // search for the first tun device
        sprintf(devicename, "/dev/tun%d", i);
        if ((tunfd = open(devicename, O_RDWR)) > 0)
            break;
    }

    infos.mtu = config.tun_mtu;
    infos.type = IFT_TUNNEL;
    infos.flags = IFF_POINTOPOINT;
    ioctl(tunfd, TUNSIFINFO, &infos);

    /* Inteface configuration */
    if (config.verbose) printf("TUN interface configuration (tun%d MTU %d)\n", i, config.tun_mtu);
    if (config.debug) printf("ifconfig...\n");
    snprintf(systemcall, 100, "ifconfig tun%d inet %s %s up", i, inet_ntoa (config.vpnIP), inet_ntoa(config.vpnIP));
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
    return close(fd);
}

/* Each frame starts with a 4 bytes header with the address family */
ssize_t read_tun(int fd, void *buf, size_t count) {
    struct iovec iov[2];
    uint32_t family;
    size_t r;

    iov[0].iov_base = &family;
    iov[0].iov_len = sizeof(family);

    iov[1].iov_base = buf;
    iov[1].iov_len = count;

    r = readv(fd, iov, 2);
    if (r > 0)
        return r - sizeof(family);
    return r;
}

ssize_t write_tun(int fd, void *buf, size_t count) {
    struct iovec iov[2];
    uint32_t family = htonl(AF_INET);

    iov[0].iov_base = &family;
    iov[0].iov_len = sizeof(family);

    iov[1].iov_base = buf;
    iov[1].iov_len = count;

    return writev(fd, iov, 2);
}

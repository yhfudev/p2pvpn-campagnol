/*
 * Create and configure a tun device
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
 */

#include "campagnol.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include "configuration.h"
#include "communication.h"
#include "tun_device.h"
#include "../common/log.h"

static char *device;

char *tun_default_up[] = {
        "ifconfig %D %V mtu %M up",
        "ip route replace %N via %V || route add -net %N gw %V",
        NULL
};
char *tun_default_down[] = {NULL};

/*
 * Open a new TUN virtual interface
 * Bind it to config.vpnIP
 * istun : use a TUN or TAP device
 */
int init_tun(int istun) {
    int tunfd;
    struct ifreq ifr;                   // interface request used to open the TUN device

    /* Open TUN interface */
    if (config.verbose) printf("TUN interface initialization\n");
    if( (tunfd = open("/dev/net/tun", O_RDWR)) < 0 ) {
         log_error("Could not open /dev/net/tun");
         return -1;
    }

    memset(&ifr, 0, sizeof(ifr));

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

    /* Inteface configuration */
    device = CHECK_ALLOC_FATAL(strdup(ifr.ifr_name));
    if (config.verbose) printf("TUN interface configuration (%s MTU %d)\n", device, config.tun_mtu);
    exec_up(device);

    return tunfd;
}

int close_tun(int fd) {
    exec_down(device);
    free(device);
    return close(fd); // the close call destroys the device
}


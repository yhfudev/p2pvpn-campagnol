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
#include "../common/log.h"

static char *device;

const char *tun_default_up[] = {
        "ifconfig %D inet %V %V up",
        "route add -net %N %V",
        NULL
};
const char *tun_default_down[] = {NULL};

/*
 * Open a new TUN virtual interface
 * Bind it to config.vpnIP
 */
int init_tun() {
    int tunfd = 0;
    int i;
    char devicename[20];
    struct tuninfo infos;

    /* Open TUN interface */
    log_message_level(1, "TUN interface initialization");
    for (i=0; i<255; i++) {
        // search for the first tun device
        snprintf(devicename, sizeof(devicename), "/dev/tun%d", i);
        if ((tunfd = open(devicename, O_RDWR)) > 0)
            break;
    }
    if (tunfd <= 0) {
        log_error(-1, "Could not open a tun device");
        return -1;
    }

    infos.mtu = config.tun_mtu;
    infos.type = IFT_TUNNEL;
    infos.flags = IFF_POINTOPOINT;
    if ((ioctl(tunfd, TUNSIFINFO, &infos)) < 0) {
        log_error(errno, "Error ioctl TUNSIFINFO");
        close(tunfd);
        return -1;
    }

    /* Inteface configuration */
    log_message_level(1, "TUN interface configuration (tun%d MTU %d)", i,
            config.tun_mtu);

    device = CHECK_ALLOC_FATAL(malloc(20));
    snprintf(device, 20, "tun%d", i);
    exec_up(device);

    return tunfd;
}

int close_tun(int fd) {
    exec_down(device);
    free(device);
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
    else if (r == -1) {
        log_error(errno, "Error while reading the tun device");
        abort();
    }

    return r;
}

ssize_t write_tun(int fd, void *buf, size_t count) {
    struct iovec iov[2];
    ssize_t r;
    uint32_t family = htonl(AF_INET);

    iov[0].iov_base = &family;
    iov[0].iov_len = sizeof(family);

    iov[1].iov_base = buf;
    iov[1].iov_len = count;

    r = writev(fd, iov, 2);
    if (r == -1) {
        log_error(errno, "Error while writting to the tun device");
        abort();
    }
    return r;
}

/*
 * This file is part of Campagnol VPN.
 * Copyright (C) 2010  Florent Bondoux
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include "configuration.h"
#include "communication.h"
#include "tun_device.h"
#include "../common/log.h"

static char *device;

const char *tun_default_up[] = {
        "ifconfig %D inet %V %V mtu %M up",
        "route add -net %N %V",
        NULL
};
const char *tun_default_down[] = {
        "ifconfig %D destroy",
        NULL
};

/*
 * Open a new TUN virtual interface
 * Bind it to config.vpnIP
 */
int init_tun() {
    int tunfd;
    char devicename[20];
    int i, v;

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

    v=0;
    if ((ioctl(tunfd, TUNSLMODE, &v)) < 0) {
        log_error(errno, "Error ioctl TUNSLMODE");
        close(tunfd);
        return -1;
    }
    if ((ioctl(tunfd, TUNSIFHEAD, &v)) < 0) {
        log_error(errno, "Error ioctl TUNSIFHEAD");
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

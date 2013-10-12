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
    struct stat buf;

    /* Open TUN interface */
    log_message_level(1, "TUN interface initialization");
    if( (tunfd = open("/dev/tun", O_RDWR)) < 0 ) {
         log_error(errno, "Could not open /dev/tun");
         return -1;
    }

    int i=0;
    if ((ioctl(tunfd, TUNSLMODE, &i)) < 0) {
        log_error(errno, "Error ioctl TUNSLMODE");
        close(tunfd);
        return -1;
    }
    if ((ioctl(tunfd, TUNSIFHEAD, &i)) < 0) {
        log_error(errno, "Error ioctl TUNSIFHEAD");
        close(tunfd);
        return -1;
    }
    fstat(tunfd, &buf);

    /* Inteface configuration */
    log_message_level(1, "TUN interface configuration (%s MTU %d)",
            devname(buf.st_rdev, S_IFCHR), config.tun_mtu);
    exec_up(devname(buf.st_rdev, S_IFCHR));

    return tunfd;
}

int close_tun(int fd) {
    struct stat buf;
    int r;

    fstat(fd, &buf);
    r = close(fd);
    exec_down(devname(buf.st_rdev, S_IFCHR));

    return r;
}


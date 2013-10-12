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

#ifndef TUN_DEVICE_H_
#define TUN_DEVICE_H_

#include "campagnol.h"
#include "../common/log.h"

extern int init_tun(void);
extern int close_tun(int fd);

extern void exec_up(const char *device);
extern void exec_down(const char *device);
extern const char *tun_default_up[];
extern const char *tun_default_down[];

#if defined (HAVE_OPENBSD)
extern ssize_t read_tun(int fd, void *buf, size_t count);
extern ssize_t write_tun(int fd, void *buf, size_t count);
#elif defined (HAVE_CYGWIN)
extern int read_tun_wait(void *buf, size_t count, unsigned long int ms);
extern ssize_t read_tun_finalize(void);
extern void read_tun_cancel(void);
extern ssize_t write_tun(void *buf, size_t count);
#else
static inline ssize_t read_tun(int fd, void *buf, size_t count) {
    ssize_t r;
    r = read(fd, buf, count);
    // We do not expect EINTR since signals are masked in this thread so any
    // error should be fatal
    if (r == -1) {
        log_error(errno, "Error while reading the tun device");
        abort();
    }
    return r;
}

static inline ssize_t write_tun(int fd, const void *buf, size_t count) {
    ssize_t r;
    r = write(fd, buf, count);
    // We do not expect any non fatal error
    if (r == -1) {
        log_error(errno, "Error while writting to the tun device");
        abort();
    }
    return r;
}
#endif

#endif /*TUN_DEVICE_H_*/

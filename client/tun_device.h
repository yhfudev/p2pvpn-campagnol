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

#ifndef TUN_DEVICE_H_
#define TUN_DEVICE_H_

extern int init_tun(int istun);
extern int close_tun(int fd);

#if defined (HAVE_FREEBSD) || defined (HAVE_LINUX)
#   define read_tun(fd,buf,count) read(fd,buf,count)
#   define write_tun(fd,buf,count) write(fd,buf,count)
#else
extern ssize_t read_tun(int fd, void *buf, size_t count);
extern ssize_t write_tun(int fd, void *buf, size_t count);
#endif

#endif /*TUN_DEVICE_H_*/

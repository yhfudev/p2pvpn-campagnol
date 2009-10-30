/*
 * Campagnol RDV server, common definitions
 *
 * Copyright (C) 2009 Florent Bondoux
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

#ifndef RDV_H_
#define RDV_H_

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_CYGWIN
#   include "lib/cygwin_byteorder.h"
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>

#define SERVER_PORT_DEFAULT 57888
#define MAX_CLIENTS_DEFAULT 100

struct configuration {
    int verbose;                                // verbose
    int debug;                                  // more verbose
    int daemonize;                              // daemonize the server
    int dump;                                   // dump packets
    uint16_t serverport;                             // RDV server port
    int max_clients;                            // maximum number of clients
    char *pidfile;                              // PID file in daemon mode
};

extern struct configuration config;
extern volatile sig_atomic_t end_server;
#define DEFAULT_PID_FILE LOCALSTATEDIR "/run/campagnol_rdv.pid"

#endif /* RDV_H_ */

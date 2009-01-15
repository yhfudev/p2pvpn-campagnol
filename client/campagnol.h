/*
 * Startup code
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

#ifndef CAMPAGNOL_H_
#define CAMPAGNOL_H_

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


/*
 * provide a dummy definition for ASSERT
 * log.h redefines ASSERT to use it with syslog
 */
#define ASSERT(expr)       assert(expr)

#include "configuration.h"

#define SERVER_PORT_DEFAULT 57888
#define DEFAULT_CONF_FILE SYSCONFDIR "/campagnol.conf"
#define DEFAULT_PID_FILE LOCALSTATEDIR "/run/campagnol.pid"

extern volatile sig_atomic_t end_campagnol;
extern struct configuration config;

#endif /*CAMPAGNOL_H_*/

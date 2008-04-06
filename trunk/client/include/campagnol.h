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

#define VERSION "0.1"

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "configuration.h"

#define SERVER_PORT_DEFAULT 57888

extern int end_campagnol;
extern struct configuration config;

#endif /*CAMPAGNOL_H_*/

/*
 * Copyright (C) 2008-2009 Florent Bondoux
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

/*
 * DTLS contexts and SSL structure management
 */

#ifndef DTLS_UTILS_H_
#define DTLS_UTILS_H_

#include "peer.h"
#include "../common/pthread_wrap.h"

extern int initDTLS(void);
extern void clearDTLS(void);
extern int rebuildDTLS(void);

extern void setup_openssl_thread(void);
extern void cleanup_openssl_thread(void);

extern int createClientSSL(struct client *peer);

#endif /* DTLS_UTILS_H_ */

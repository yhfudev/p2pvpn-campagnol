/*
 * OpenSSL rate limiter filter BIO
 *
 * Copyright (C) 2008-2011 Florent Bondoux
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

#ifndef BF_RATE_LIMITER_H_
#define BF_RATE_LIMITER_H_

#include <openssl/bio.h>
#include "rate_limiter.h"

/* BIO type: filter */
#define BIO_TYPE_RATE_FILTER    (101|BIO_TYPE_FILTER)

/* Create a new BIO */
extern BIO *BIO_f_new_rate_limiter(struct tb_state*, struct tb_state*);

struct rate_limiter_data {
    struct tb_state *global;
    struct tb_state *client;
};


#endif /* BF_RATE_LIMITER_H_ */

/*
 * rate limiter
 *
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

#ifndef RATE_LIMITING_H_
#define RATE_LIMITING_H_

#include <stdlib.h>
#include <time.h>
#include "../common/pthread_wrap.h"

struct tb_state {
    size_t bucket_size;         // bucket size (byte)
    double bucket_rate;         // token arrival rate (kByte/s)
    size_t bucket_available;    // number of tokens (byte)
    struct timespec last_arrival_time;
    size_t packet_overhead;     // add overhead to each counted packets
    pthread_mutex_t mutex;
    int lock;                   // lock the mutex befor counting
};

extern void tb_init(struct tb_state *, size_t, double, size_t, int);
extern void tb_clean(struct tb_state *);
extern void tb_count(struct tb_state *, size_t);


#endif /* RATE_LIMITING_H_ */

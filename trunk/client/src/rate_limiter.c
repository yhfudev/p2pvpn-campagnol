/*
 * rate limiter
 *
 * Copyright (C) 2008 Florent Bondoux
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
 * This file implements a small token bucket
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#include "log.h"
#include "rate_limiter.h"

/*
 * initialize a token bucket
 *
 * size: size of the bucket in bytes
 * rate: bucket's refill rate (kBytes/s)
 */
void tb_init(struct tb_state * tb, size_t size, double rate) {
    tb->bucket_size = size;
    tb->bucket_available = tb->bucket_size;
    tb->bucket_rate = rate;
    gettimeofday(&tb->last_arrival_time, NULL);
}

/*
 * count a packet
 * wait untill the bucket contains enough tokens
 *
 * packet_size: size of the packet in bytes
 */
void tb_count(struct tb_state *tb, size_t packet_size) {
    struct timeval now, elapsed;
    struct timespec req_sleep, rem_sleep;
    double elapsed_ms, sleep_ms;
    int r;

    ASSERT(packet_size <= tb->bucket_size);

    /* refill the bucket with the number of tokens added since the last call */
    gettimeofday(&now, NULL);

    timersub(&now, &tb->last_arrival_time, &elapsed);
    elapsed_ms = elapsed.tv_sec*1000 + (elapsed.tv_usec/1000.);

    tb->bucket_available += (size_t) round(elapsed_ms * tb->bucket_rate);
    tb->bucket_available = (tb->bucket_available > tb->bucket_size) ? tb->bucket_size : tb->bucket_available;

    /* update last arrival time */
    memcpy(&tb->last_arrival_time, &now, sizeof(tb->last_arrival_time));

    /* ok */
    if (packet_size <= tb->bucket_available) {
        tb->bucket_available -= packet_size;
        return;
    }

    /* not enough tokens ?
     * we need to sleep during sleep_ms
     */
    sleep_ms = (packet_size - tb->bucket_available) / tb->bucket_rate;
    req_sleep.tv_sec = (time_t) floor(sleep_ms/1000.);
    req_sleep.tv_nsec = (long int) ((sleep_ms - 1000 * req_sleep.tv_sec) * 1000000L);

    while ((r = nanosleep(&req_sleep, &rem_sleep)) != 0 ) {
        if (errno == EINTR) {
            memcpy(&req_sleep, &rem_sleep, sizeof(req_sleep));
        }
        else {
            log_error("nanosleep");
            break;
        }
    }

    /* refill the bucket */
    gettimeofday(&now, NULL);

    timersub(&now, &tb->last_arrival_time, &elapsed);
    elapsed_ms = elapsed.tv_sec*1000 + (elapsed.tv_usec/1000.);

    tb->bucket_available += (size_t) round(elapsed_ms * tb->bucket_rate);
    tb->bucket_available = (tb->bucket_available < packet_size) ? 0 : (tb->bucket_available - packet_size);
    tb->bucket_available = (tb->bucket_available > tb->bucket_size) ? tb->bucket_size : tb->bucket_available;

    memcpy(&tb->last_arrival_time, &now, sizeof(tb->last_arrival_time));
}

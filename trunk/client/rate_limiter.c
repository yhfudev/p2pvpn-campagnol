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
#include <unistd.h>

#include "../common/log.h"
#include "rate_limiter.h"

#ifdef _POSIX_MONOTONIC_CLOCK
#define SHAPER_CLOCK CLOCK_MONOTONIC
#else
#define SHAPER_CLOCK CLOCK_REALTIME
#endif

#ifndef timespecsub
#   define timespecsub(a, b, result)                        \
    do {                                                    \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;       \
        (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec;    \
        if ((result)->tv_nsec < 0) {                        \
            --(result)->tv_sec;                             \
            (result)->tv_nsec += 1000000000L;               \
        }                                                   \
    } while (0)
#endif

/*
 * initialize a token bucket
 *
 * size: size of the bucket in bytes
 * rate: bucket's refill rate (kBytes/s)
 * overhead: add overhead to each packet (UDP header...)
 * with_lock: protect tb_count with a mutex
 */
void tb_init(struct tb_state * tb, size_t size, double rate, size_t overhead, int with_lock) {
    tb->bucket_size = size;
    tb->bucket_available = tb->bucket_size;
    tb->bucket_rate = rate;
    tb->packet_overhead = overhead;
    clock_gettime(SHAPER_CLOCK, &tb->last_arrival_time);
    tb->lock = with_lock;
    if (with_lock)
        mutexInit(&tb->mutex, NULL);
}

void tb_clean(struct tb_state * tb) {
    if (tb->lock)
        mutexDestroy(&tb->mutex);
}

/*
 * count a packet
 * wait untill the bucket contains enough tokens
 *
 * packet_size: size of the packet in bytes
 */
void tb_count(struct tb_state *tb, size_t packet_size) {
    struct timespec now, elapsed;
    struct timespec req_sleep, rem_sleep;
    double elapsed_ms, sleep_ms;

    if (tb->lock) {
        mutexLock(&tb->mutex);
    }

    packet_size += tb->packet_overhead;

    ASSERT(packet_size <= tb->bucket_size);

    /* refill the bucket with the number of tokens added since the last call */
    clock_gettime(SHAPER_CLOCK, &now);

    timespecsub(&now, &tb->last_arrival_time, &elapsed);
    elapsed_ms = (double) elapsed.tv_sec * 1000. + ((double) elapsed.tv_nsec/1000000.);

    tb->bucket_available += (size_t) round(elapsed_ms * tb->bucket_rate);
    tb->bucket_available = (tb->bucket_available > tb->bucket_size) ? tb->bucket_size : tb->bucket_available;

    /* update last arrival time */
    memcpy(&tb->last_arrival_time, &now, sizeof(tb->last_arrival_time));

    /* ok */
    if (packet_size <= tb->bucket_available) {
        tb->bucket_available -= packet_size;
        if (tb->lock) {
            mutexUnlock(&tb->mutex);
        }
        return;
    }

    /* not enough tokens ?
     * we need to sleep during sleep_ms
     */
    sleep_ms = (double) (packet_size - tb->bucket_available) / tb->bucket_rate;
    req_sleep.tv_sec = (time_t) floor(sleep_ms/1000.);
    req_sleep.tv_nsec = (long int) ((sleep_ms - 1000. * (double) req_sleep.tv_sec) * 1000000L);

    while (nanosleep(&req_sleep, &rem_sleep) != 0 ) {
        if (errno == EINTR) {
            memcpy(&req_sleep, &rem_sleep, sizeof(req_sleep));
        }
        else {
            log_error(errno, "nanosleep");
            break;
        }
    }

    /* refill the bucket */
    clock_gettime(SHAPER_CLOCK, &now);

    timespecsub(&now, &tb->last_arrival_time, &elapsed);
    elapsed_ms = (double) elapsed.tv_sec * 1000. + ((double) elapsed.tv_nsec/1000000.);

    tb->bucket_available += (size_t) round(elapsed_ms * tb->bucket_rate);
    tb->bucket_available = (tb->bucket_available < packet_size) ? 0 : (tb->bucket_available - packet_size);
    tb->bucket_available = (tb->bucket_available > tb->bucket_size) ? tb->bucket_size : tb->bucket_available;

    memcpy(&tb->last_arrival_time, &now, sizeof(tb->last_arrival_time));
    if (tb->lock) {
        mutexUnlock(&tb->mutex);
    }
}

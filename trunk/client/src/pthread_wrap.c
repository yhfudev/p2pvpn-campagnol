/*
 * Pthread wrapper functions
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
 * Wrapper functions around a few pthread functions
 * These functions exit in case of error
 */

#include "config.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "pthread_wrap.h"
#include "log.h"

void mutexInit(pthread_mutex_t *mutex, pthread_mutexattr_t *attrs) {
    ASSERT(mutex);
    int r = pthread_mutex_init(mutex, attrs);
    if (r != 0) {
        log_message("Error pthread_mutex_init(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

void mutexDestroy(pthread_mutex_t *mutex) {
    ASSERT(mutex);
    int r = pthread_mutex_destroy(mutex);
    if (r != 0) {
        log_message("Error pthread_mutex_destroy(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

void mutexLock(pthread_mutex_t *mutex) {
    ASSERT(mutex);
    int r = pthread_mutex_lock(mutex);
    if (r != 0) {
        log_message("Error pthread_mutex_lock(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

void mutexUnlock(pthread_mutex_t *mutex) {
    ASSERT(mutex);
    int r = pthread_mutex_unlock(mutex);
    if (r != 0) {
        log_message("Error pthread_mutex_unlock(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

void mutexattrInit(pthread_mutexattr_t *attrs) {
    ASSERT(attrs);
    int r = pthread_mutexattr_init(attrs);
    if (r != 0) {
        log_message("Error pthread_mutexattr_init(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

void mutexattrSettype(pthread_mutexattr_t *attrs, int type) {
    ASSERT(attrs);
    int r = pthread_mutexattr_settype(attrs, type);
    if (r != 0) {
        log_message("Error pthread_mutexattr_settype(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

void mutexattrDestroy(pthread_mutexattr_t *attrs) {
    ASSERT(attrs);
    int r = pthread_mutexattr_destroy(attrs);
    if (r != 0) {
        log_message("Error pthread_mutexattr_destroy(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

void conditionInit(pthread_cond_t *cond, pthread_condattr_t *attrs) {
    ASSERT(cond);
    int r = pthread_cond_init(cond, attrs);
    if (r != 0) {
        log_message("Error pthread_cond_init(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

void conditionDestroy(pthread_cond_t *cond) {
    ASSERT(cond);
    int r = pthread_cond_destroy(cond);
    if (r != 0) {
        log_message("Error pthread_cond_destroy(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

int conditionWait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    ASSERT(cond);
    int retval;
    retval = pthread_cond_wait(cond, mutex);
    if (retval != 0) {
        log_message("Error pthread_cond_wait(): %s", strerror(retval));
        exit(EXIT_FAILURE);
    }
    return retval;
}

int conditionTimedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
        const struct timespec *abs_timeout) {
    ASSERT(cond);
    ASSERT(abs_timeout);
    int retval;
    retval = pthread_cond_timedwait(cond, mutex, abs_timeout);
    if (retval != 0 && retval != ETIMEDOUT) {
        log_message("Error pthread_cond_timedwait(): %s", strerror(retval));
        exit(EXIT_FAILURE);
    }
    return retval;
}

int conditionBroadcast(pthread_cond_t *cond) {
    ASSERT(cond);
    int retval;
    retval = pthread_cond_broadcast(cond);
    if (retval != 0) {
        log_message("Error pthread_cond_broadcast(): %s", strerror(retval));
        exit(EXIT_FAILURE);
    }
    return retval;
}

int conditionSignal(pthread_cond_t *cond) {
    ASSERT(cond);
    int retval;
    retval = pthread_cond_signal(cond);
    if (retval != 0) {
        log_message("Error pthread_cond_signal(): %s", strerror(retval));
        exit(EXIT_FAILURE);
    }
    return retval;
}

/*
 * Create a thread executing start_routine with the arguments arg
 * without attributes
 */
pthread_t createThread(void * (*start_routine)(void *), void * arg) {
    ASSERT(start_routine);
    pthread_t thread;
    if (pthread_create(&thread, NULL, start_routine, arg) != 0) {
        log_error("Error pthread_create()");
        exit(EXIT_FAILURE);
    }
    return thread;
}

/*
 * Create a thread executing start_routine with the arguments arg
 * without attributes. Then call pthread_detach.
 */
pthread_t createDetachedThread(void * (*start_routine)(void *), void * arg) {
    pthread_t thread = createThread(start_routine, arg);
    if (pthread_detach(thread) != 0) {
        log_error("Error pthread_detach()");
        exit(EXIT_FAILURE);
    }
    return thread;
}

void joinThread(pthread_t thread, void **value_ptr) {
    int r = pthread_join(thread, value_ptr);
    if (r != 0) {
        log_message("Error pthread_join(): %s", strerror(r));
        exit(EXIT_FAILURE);
    }
}

/*
 * Semaphore wrapper functions
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
 */

#ifndef SEM_WRAP_H_
#define SEM_WRAP_H_

#include "config.h"
#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "log.h"

static inline void semInit(sem_t *sem, int pshared, unsigned int value);
static inline void semDestroy(sem_t *sem);
static inline int semWait(sem_t *sem);
static inline int semTimedwait(sem_t *sem, const struct timespec *abs_timeout);
static inline int semTrywait(sem_t *sem);
static inline void semPost(sem_t *sem);
static inline void semGetValue(sem_t *sem, int *sval);

void semInit(sem_t *sem, int pshared, unsigned int value) {
    ASSERT(sem);
    int r = sem_init(sem, pshared, value);
    if (r != 0) {
        log_error(errno, "Error sem_init()");
        abort();
    }
}

void semDestroy(sem_t *sem) {
    ASSERT(sem);
    int r = sem_destroy(sem);
    if (r != 0) {
        log_error(errno, "Error sem_destroy()");
        abort();
    }
}

int semWait(sem_t *sem) {
    ASSERT(sem);
#ifdef HAVE_LINUX
    int r;
    // sem_wait is not restarted after a signal or STOP/CONT for linux <= 2.6.21
    do r = sem_wait(sem);
    while (r != 0 && errno == EINTR);
#else
    int r = sem_wait(sem);
#endif
    if (r != 0) {
        log_error(errno, "Error sem_wait()");
        abort();
    }
    return r;
}

int semTimedwait(sem_t *sem, const struct timespec *abs_timeout) {
    ASSERT(sem);
    ASSERT(abs_timeout);
#ifdef HAVE_LINUX
    int r;
    do r = sem_timedwait(sem, abs_timeout);
    while (r != 0 && errno == EINTR);
#else
    int r = sem_timedwait(sem, abs_timeout);
#endif
    if (r != 0 && errno != ETIMEDOUT) {
        log_error(errno, "Error sem_timedwait()");
        abort();
    }
    return r;
}

int semTrywait(sem_t *sem) {
    ASSERT(sem);
#ifdef HAVE_LINUX
    int r;
    // sem_wait is not restarted after a signal or STOP/CONT for linux <= 2.6.21
    do r = sem_trywait(sem);
    while (r != 0 && errno == EINTR);
#else
    int r = sem_trywait(sem);
#endif
    if (r != 0 && errno != EAGAIN) {
        log_error(errno, "Error sem_trywait()");
        abort();
    }
    return r;
}

void semPost(sem_t *sem) {
    ASSERT(sem);
    int r = sem_post(sem);
    if (r != 0) {
        log_error(errno, "Error sem_post()");
        abort();
    }
}

void semGetValue(sem_t *sem, int *sval) {
    ASSERT(sem);
    ASSERT(sval);
    int r = sem_getvalue(sem, sval);
    if (r != 0) {
        log_error(errno, "Error sem_getvalue()");
        abort();
    }
}

#endif /* SEM_WRAP_H_ */

/*
 * Semaphore wrapper functions
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

#ifndef SEM_WRAP_H_
#define SEM_WRAP_H_

#include "config.h"
#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "../common/log.h"

static inline void semInit(sem_t *sem, int pshared, unsigned int value);
static inline void semDestroy(sem_t *sem);
static inline void semWait(sem_t *sem);
static inline int semTimedwait(sem_t *sem, const struct timespec *abs_timeout);
static inline void semPost(sem_t *sem);
static inline void semGetValue(sem_t *sem, int *sval);

void semInit(sem_t *sem, int pshared, unsigned int value) {
    ASSERT(sem);
    int r = sem_init(sem, pshared, value);
    if (r != 0) {
        log_message("Error sem_init(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void semDestroy(sem_t *sem) {
    ASSERT(sem);
    int r = sem_destroy(sem);
    if (r != 0) {
        log_message("Error sem_destroy(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void semWait(sem_t *sem) {
    ASSERT(sem);
    int r = sem_wait(sem);
    if (r != 0) {
        log_message("Error sem_wait(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int semTimedwait(sem_t *sem, const struct timespec *abs_timeout) {
    ASSERT(sem);
    ASSERT(abs_timeout);
    int r = sem_timedwait(sem, abs_timeout);
    if (r != 0 && errno != ETIMEDOUT) {
        log_message("Error sem_timedwait(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return r;
}

void semPost(sem_t *sem) {
    ASSERT(sem);
    int r = sem_post(sem);
    if (r != 0) {
        log_message("Error sem_post(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void semGetValue(sem_t *sem, int *sval) {
    ASSERT(sem);
    ASSERT(sval);
    int r = sem_getvalue(sem, sval);
    if (r != 0) {
        log_message("Error sem_getvalue(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

#endif /* SEM_WRAP_H_ */
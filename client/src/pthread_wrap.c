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
#include "campagnol.h"
#include "pthread_wrap.h"
#include "log.h"


void mutexLock(pthread_mutex_t *mutex) {
    if (mutex) {
        if ( pthread_mutex_lock(mutex) != 0 ) {
            log_error("Error pthread_mutex_lock()");
            exit(1);
        }
    }
    else {
        log_message("Call mutexLock with a NULL mutex");
        exit(1);
    }
};


void mutexUnlock(pthread_mutex_t *mutex) {
    if (mutex) {
        if ( pthread_mutex_unlock(mutex) != 0 ) {
            log_error("Error pthread_mutex_unlock()");
            exit(1);
        }
    }
    else {
        log_message("Call mutexUnlock with a NULL mutex");
        exit(1);
    }
};


pthread_cond_t createCondition(void) {
    pthread_cond_t cond;
    if ( pthread_cond_init(&cond, NULL) != 0 ) {
        log_error("Error pthread_cond_init()");
        exit(1);
    }
    return cond;
}

void destroyCondition(pthread_cond_t *cond) {
    if (cond) {
        pthread_cond_destroy(cond);
    }
}

int conditionWait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (cond) {
        int retval;
        retval =  pthread_cond_wait(cond, mutex);
        if ( retval != 0) {
            log_error("Error pthread_cond_wait()");
            exit(1);
        }
        return retval;
    }
    else {
        log_message("Call conditionWait with a NULL condition");
        exit(1);
    }
}

int conditionTimedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abs_timeout) {
    if (cond) {
        int retval;
        retval =  pthread_cond_timedwait(cond, mutex, abs_timeout);
        if ( retval != 0 && retval != ETIMEDOUT) {
            log_error("Error pthread_cond_timedwait()");
            exit(1);
        }
        return retval;
    }
    else {
        log_message("Call conditionTimedwait with a NULL condition");
        exit(1);
    }
}

int conditionBroadcast(pthread_cond_t *cond) {
    if (cond) {
        int retval;
        retval =  pthread_cond_broadcast(cond);
        if ( retval != 0) {
            log_error("Error pthread_cond_broadcast()");
            exit(1);
        }
        return retval;
    }
    else {
        log_message("Call conditionBroadcat with a NULL semaphore");
        exit(1);
    }
}

int conditionSignal(pthread_cond_t *cond) {
    if (cond) {
        int retval;
        retval =  pthread_cond_signal(cond);
        if ( retval != 0) {
            log_error("Error pthread_cond_signal()");
            exit(1);
        }
        return retval;
    }
    else {
        log_message("Call conditionSignal with a NULL semaphore");
        exit(1);
    }
}

/*
 * Create a thread executing start_routine with the arguments arg
 * without attributes
 */
pthread_t createThread(void * (*start_routine)(void *), void * arg) {
    pthread_t thread;
    if ( pthread_create(&thread, NULL, start_routine, arg) != 0 ) {
        log_error("Error pthread_create()");
        exit(1);
    }
    return thread;
};

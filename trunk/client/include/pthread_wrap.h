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

#ifndef PTHREAD_WRAP_H_
#define PTHREAD_WRAP_H_

#include <pthread.h>

extern void mutexInit(pthread_mutex_t *mutex, pthread_mutexattr_t *attrs);
extern void mutexDestroy(pthread_mutex_t *mutex);
extern void mutexLock(pthread_mutex_t *mutex);
extern void mutexUnlock(pthread_mutex_t *mutex);

extern void mutexattrInit(pthread_mutexattr_t *attrs);
extern void mutexattrDestroy(pthread_mutexattr_t *attrs);
extern void mutexattrSettype(pthread_mutexattr_t *attrs, int type);

extern void conditionInit(pthread_cond_t *cond, pthread_condattr_t *attrs);
extern void conditionDestroy(pthread_cond_t *cond);
extern int conditionWait(pthread_cond_t *cond, pthread_mutex_t *mutex);
extern int conditionTimedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abs_timeout);
extern int conditionBroadcast(pthread_cond_t *cond);
extern int conditionSignal(pthread_cond_t *cond);

extern pthread_t createThread(void * (*start_routine)(void *), void * arg);
extern void joinThread(pthread_t thread, void **value_ptr);

#endif /*PTHREAD_WRAP_H_*/

/*
 * Small log functions
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

#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>

extern void log_init(int enabled, int level, const char *name);
extern void log_close(void);
extern __attribute__((format(printf,1,2))) void log_message(const char *format,
        ...);
extern __attribute__((format(printf,2,3))) void log_message_level(
        int level, const char *format, ...);
extern __attribute__((format(printf,1,2))) void log_message_syslog(
        const char *format, ...);
extern __attribute__((format(printf,5,6))) void _log_error(const char *filename,
        unsigned int linenumber, const char *functionname, int error_code,
        const char *format, ...);
#define log_error(error_code, format, ...) \
    _log_error(__FILE__, __LINE__, __func__, error_code, format, ##__VA_ARGS__)
#ifdef HAVE_CYGWIN
extern __attribute__((format(printf,5,6))) void _log_error_cygwin(
        const char *filename, unsigned int linenumber, const char *functionname,
        int error_code, const char *format, ...);
#define log_error_cygwin(error_code, format, ...)\
    _log_error_cygwin(__FILE__, __LINE__, __func__, error_code, format,\
            ##__VA_ARGS__)
#endif

/* log and exit(1) if __ptr == NULL
 * return __ptr otherwise
 */
#define CHECK_ALLOC_FATAL(__ptr)    ({\
    void *ptr = __ptr;  \
    if (ptr == NULL)  {\
        log_error(errno, "Fatal error");    \
        exit(EXIT_FAILURE); \
    }   \
    ptr;    \
    })


/*
 * ASSERT: if campagnol run as a daemon, log the assertion error message with
 * syslog
 * otherwise, same as assert.
 */
#ifdef ASSERT
#   undef ASSERT
#endif
#ifdef NDEBUG
#   define ASSERT(expr)       ((void)(0))
#else
#   include <assert.h>
#   ifndef __STRING
#       define __STRING(x)  #x
#   endif
#   define assert_log(expr)             \
    ((expr)                         \
        ? (void)(0)    \
        : log_message_syslog("%s:%d: %s: Assertion `%s' failed.", __FILE__, __LINE__, __func__, __STRING(expr)) \
    )
#   define ASSERT(expr)       {assert_log(expr);assert(expr);}
#endif

#endif /*LOG_H_*/

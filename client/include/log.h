/*
 * Small log functions
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

#ifndef LOG_H_
#define LOG_H_

extern void log_init(int enabled, int verbose, const char *name);
extern void log_close(void);
extern void log_message(const char *format, ...);
extern void log_message_verb(const char *format, ...);
extern void log_message_syslog(const char *format, ...);
extern void __log_error(const char *filename, unsigned int linenumber, const char *functionname, const char *s);
#define log_error(msg)      __log_error(__FILE__, __LINE__, __func__, msg)

/*
 * ASSERT: if campagnol run as a daemon, log the assertion error message with syslog
 * otherwise, same as assert.
 */
#ifdef ASSERT
#   undef ASSERT
#endif
#ifdef NDEBUG
#   define ASSERT(expr)       ((void)(0))
#else
#   include <assert.h>
#   define assert_log(expr)             \
    ((expr)                         \
        ? (void)(0)    \
        : log_message_syslog("%s:%d: %s: Assertion `%s' failed.", __FILE__, __LINE__, __func__, __STRING(expr)) \
    )
#   define ASSERT(expr)       {assert_log(expr);assert(expr);}
#endif

#endif /*LOG_H_*/

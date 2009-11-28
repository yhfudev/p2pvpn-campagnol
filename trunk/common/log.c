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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"
#include "strlib.h"

static int log_enabled;
static int log_level;

/*
 * Initialise syslog if enabled is true
 * level is the minimum level for log_message_level
 * Use name for the logs
 */
void log_init(int enabled, int level, const char *name) {
    log_enabled = enabled;
    log_level = level;
    if (log_enabled) {
        openlog(name, LOG_PID, LOG_DAEMON);
    }
}

/*
 * Optional
 * Close syslog
 */
void log_close(void) {
    closelog();
}

/*
 * Log format/ap if log_enabled is true
 * Otherwise print the message to out if not NULL
 */
__attribute__((format(printf,2,0))) static inline void log_message_inner_v(
        FILE *out, const char *format, va_list ap) {
    if (log_enabled) {
        vsyslog(LOG_NOTICE, format, ap);
    }
    else if (out) {
        vfprintf(out, format, ap);
        fprintf(out, "\n");
    }
}

/*
 * Equivalent to log_message_inner_v but called with a variable argument list
 */
__attribute__((format(printf,2,3))) static inline void log_message_inner(
        FILE *out, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    log_message_inner_v(out, format, ap);
    va_end(ap);
}

/*
 * Log a message with syslog or print it to stdout
 */
void log_message(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    log_message_inner_v(stdout, format, ap);
    va_end(ap);
}

/*
 * If level <= log_level, log a message with syslog or print it to stdout
 */
void log_message_level(int level, const char *format, ...) {
    va_list ap;
    if (level <= log_level) {
        va_start(ap, format);
        log_message_inner_v(stdout, format, ap);
        va_end(ap);
    }
}

/*
 * Log a message with syslog
 */
void log_message_syslog(const char *format, ...) {
    va_list ap;
    if (log_enabled) {
        va_start(ap, format);
        vsyslog(LOG_NOTICE, format, ap);
        va_end(ap);
    }
}

/*
 * Print the last error (strerrno) with syslog if log_enabled is true
 * or to stderr
 */
void _log_error(const char *filename, unsigned int linenumber,
        const char *functionname, int error_code, const char *format, ...) {
    char *error_str = NULL;
    va_list ap;
    strlib_buf_t sb;

    if (error_code != -1) {
#ifdef HAVE_STRERROR_R
        char error_buf[512];

        /* sterror is available in two versions: POSIX or GNU */
#ifdef STRERROR_R_CHAR_P /* GNU */
        error_str = strerror_r(error_code, error_buf, 512);
#else /* POSIX */
        if (strerror_r(error_code, error_buf, 512) != 0) {
            perror("strerror_r");
            strncpy(error_buf, "Could not get the error message", 512);
        }
        error_str = error_buf;
#endif /* STRERROR_R_CHAR_P */
#else /* HAVE_STRERROR_R */
        error_str = strerror(error_code);
#endif /* HAVE_STRERROR_R */
    }

    strlib_init(&sb);
    strlib_appendf(&sb, "%s:%u:%s: ", filename, linenumber, functionname);
    if (format != NULL) {
        va_start(ap, format);
        strlib_vappendf(&sb, format, ap);
        va_end(ap);
    }
    else {
        strlib_appendf(&sb, "%s", "Error");
    }

    if (error_code != -1)
        log_message_inner(stderr, "%s: %s", sb.s, error_str);
    else
        log_message_inner(stderr, "%s", sb.s);

    strlib_free(&sb);
}

#ifdef HAVE_CYGWIN
#   include <w32api/windows.h>

void _log_error_cygwin(const char *filename, unsigned int linenumber,
        const char *functionname, int error_code, const char *format, ...) {
    char error_str[1024];
    va_list ap;
    strlib_buf_t sb;

    if (error_code != -1) {
        if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        error_str, sizeof(error_str), NULL)) {
            strncpy(error_str, "Could not get the error message", sizeof(error_str));
        }
    }

    strlib_init(&sb);
    strlib_appendf(&sb, "%s:%u:%s: ", filename, linenumber, functionname);
    if (format != NULL) {
        va_start(ap, format);
        strlib_vappendf(&sb, format, ap);
        va_end(ap);
    }
    else {
        strlib_appendf(&sb, "%s", "Error");
    }

    if (error_code != -1)
        log_message_inner(stderr, "%s: %s", sb.s, error_str);
    else
        log_message_inner(stderr, "%s", sb.s);

    strlib_free(&sb);
}
#endif

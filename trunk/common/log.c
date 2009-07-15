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

static int log_enabled;
static int log_verbose;

/*
 * Initialise syslog if enabled is true
 * Use name for the logs
 */
void log_init(int enabled, int verbose, const char *name) {
    log_enabled = enabled;
    log_verbose = verbose;
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
__attribute__((format(printf,2,0))) static inline void log_message_inner(
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
 * Log a message with systlog or print it to out
 */
void _log_message(FILE *out, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    log_message_inner(out, format, ap);
    va_end(ap);
}

/*
 * Log a message with syslog or print it to stdout
 * if log_verbose is true
 */
void log_message_verb(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    log_message_inner(log_verbose ? stdout : NULL, format, ap);
    va_end(ap);
}

/*
 * Log a message with syslog
 */
void log_message_syslog(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    if (log_enabled) {
        vsyslog(LOG_NOTICE, format, ap);
    }
    va_end(ap);
}

/*
 * Print the last error (strerrno) with syslog if log_enabled is true
 * or to stderr
 */
void _log_error(const char *filename, unsigned int linenumber,
        const char *functionname, int error_code, const char *s) {
    char *error_str = NULL;

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

    if (s) {
        if (error_code != -1)
            _log_message(stderr, "%s:%u:%s: %s: %s", filename, linenumber,
                    functionname, s, error_str);
        else
            _log_message(stderr, "%s:%u:%s: %s", filename, linenumber,
                    functionname, s);
    }
    else {
        if (error_code != -1)
            _log_message(stderr, "%s:%u:%s: %s", filename, linenumber,
                    functionname, error_str);
        else
            _log_message(stderr, "%s:%u:%s: Error", filename, linenumber,
                    functionname);
    }
}

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

#include <syslog.h>
#include <stdarg.h>

#include "campagnol.h"
#include "log.h"
#include "configuration.h"

static int log_enabled;

/*
 * Initialise syslog if enabled is true
 * Use name for the logs
 */
void log_init(int enabled, const char *name) {
    log_enabled = enabled;
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
 * Otherwise print the message to stderr if tostderr is true
 */
inline void log_message_inner(int tostderr, const char *format, va_list ap) {
    if (log_enabled) {
        vsyslog(LOG_NOTICE, format, ap);
    }
    else if (tostderr) {
        vfprintf(stderr, format, ap);
        fprintf(stderr, "\n");
    }
}

/*
 * Log a message with systlog or print it to stderr
 */
void log_message(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    log_message_inner(1, format, ap);
    va_end(ap);
}

/*
 * Log a message with systlog or print it to stderr
 * if config.verbose is true
 */
void log_message_verb(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    log_message_inner(config.verbose, format, ap);
    va_end(ap);
}

/*
 * Print the last error (strerrno) with syslog if log_enabled is true
 * Call perror otherwise
 */
void log_error(const char *s) {
    if (log_enabled) {
        syslog(LOG_NOTICE, "%s: %s", s, strerror(errno));
    }
    else {
        perror(s);
    }
}

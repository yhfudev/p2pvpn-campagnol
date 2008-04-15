#include <syslog.h>
#include <stdarg.h>

#include "campagnol.h"
#include "log.h"
#include "configuration.h"

static int log_enabled;

void log_init(int enabled, const char *name) {
    log_enabled = enabled;
    if (log_enabled) {
        openlog(name, LOG_PID, LOG_DAEMON);
    }
}

void log_close(void) {
    closelog();
}

inline void log_message_inner(int tostderr, const char *format, va_list ap) {
    if (log_enabled) {
        vsyslog(LOG_NOTICE, format, ap);
    }
    else if (tostderr) {
        vfprintf(stderr, format, ap);
        fprintf(stderr, "\n");
    }
}

void log_message(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    log_message_inner(1, format, ap);
    va_end(ap);
}

void log_message_verb(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    log_message_inner(config.verbose, format, ap);
    va_end(ap);
}

void log_error(const char *s) {
    if (log_enabled) {
        syslog(LOG_NOTICE, "%s: %s", s, strerror(errno));
    }
    else {
        perror(s);
    }
}

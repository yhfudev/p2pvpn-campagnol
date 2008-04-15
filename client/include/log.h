#ifndef LOG_H_
#define LOG_H_

extern void log_init(int enabled, const char *name);
extern void log_close(void);
extern void log_message(const char *format, ...);
extern void log_message_verb(const char *format, ...);
extern void log_error(const char *s);

#endif /*LOG_H_*/

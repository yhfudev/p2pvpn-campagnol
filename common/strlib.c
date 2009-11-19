/*
 * This file is part of Campagnol VPN.
 * Copyright (C) 2009  Florent Bondoux
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include "strlib.h"

// The log API uses the library, so we don't use log.h
// Prefix the perror argument with the file and line number
#define _PERROR_QUOTE(x) #x
#define PERROR_QUOTE(x) _PERROR_QUOTE(x)
#define PERROR(str) perror(__FILE__ ":" PERROR_QUOTE(__LINE__) ": " str)

/*
 * Init a string buffer.
 * The buffer is valid and nul-terminated
 */
void strlib_init(strlib_buf_t *sb) {
    sb->len = 0;
    sb->buflen = 0;
    sb->mark = 0;
    sb->s = NULL;
    strlib_grow(sb, 1);
    sb->s[0] = '\0';
}

/*
 * Free the buffer
 */
void strlib_free(strlib_buf_t *sb) {
    free(sb->s);
    sb->s = NULL;
    sb->len = 0;
    sb->buflen = 0;
}

/*
 * Erase the string
 */
void strlib_reset(strlib_buf_t *sb) {
    sb->len = 0;
    sb->mark = 0;
    sb->s[sb->len] = '\0';
}

/*
 * Set the mark for strlib_rstrip
 */
void strlib_setmark(strlib_buf_t *sb, size_t mark) {
    sb->mark = mark;
}

/*
 * Grow the buffer so that it's possible to append a string of length n
 */
void strlib_grow(strlib_buf_t *sb, size_t n) {
    size_t buflen_new;
    size_t buflen_grow;
    buflen_new = sb->len + n + 1;
    if (buflen_new > sb->buflen) {
        // This growing factor is from git's cache.h
        buflen_grow = (sb->buflen + 16) * 3 / 2;
        if (buflen_grow < buflen_new)
            sb->buflen = buflen_new;
        else
            sb->buflen = buflen_grow;
        sb->s = realloc(sb->s, sb->buflen);
        if (sb->s == NULL) {
            PERROR("realloc");
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * Append a single char
 */
void strlib_push(strlib_buf_t *sb, char c) {
    strlib_grow(sb, 1);
    sb->s[sb->len] = c;
    sb->len++;
    sb->s[sb->len] = '\0';
}

/*
 * Append a string of length len
 */
void strlib_append(strlib_buf_t *sb, const char *s, size_t len) {
    strlib_grow(sb, len);
    memcpy(sb->s + sb->len, s, len);
    sb->len += len;
    sb->s[sb->len] = '\0';
}

/*
 * Remove the trailing blank (space or tab) characters.
 * Stop at the first non blank char or at the mark
 */
void strlib_rstrip(strlib_buf_t *sb) {
    while (sb->len > 0 && sb->len > sb->mark && isblank(sb->s[sb->len - 1]))
        sb->len--;
    sb->s[sb->len] = '\0';
}

/*
 * Append src to dest
 */
void strlib_appendbuf(strlib_buf_t *dest, strlib_buf_t *src) {
    strlib_append(dest, src->s, src->len);
}

/* Append a formated string to the buffer */
void strlib_appendf(strlib_buf_t *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    strlib_vappendf(sb, fmt, ap);
    va_end(ap);
}

/* Append a formated string to the buffer with a va_list */
void strlib_vappendf(strlib_buf_t *sb, const char *fmt, va_list ap) {
    int len;
    va_list aq;
    va_copy(aq, ap);
    len = vsnprintf(sb->s + sb->len, sb->buflen - sb->len, fmt, aq);
    va_end(aq);
    if (len < 0) {
        PERROR("vsnprintf");
        exit(EXIT_FAILURE);
    }
    if (len >= (int) (sb->buflen - sb->len)) {
        strlib_grow(sb, len);
        va_copy(aq, ap);
        len = vsnprintf(sb->s + sb->len, sb->buflen - sb->len, fmt, ap);
        va_end(aq);
    }
    sb->len += len;
}

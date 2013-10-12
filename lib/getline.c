/*
 * getline replacement
 *
 * Copyright (C) 2009 Florent Bondoux
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
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * 
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 *
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>

#include "getline.h"

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    assert(lineptr);
    assert(n);
    assert(stream);
    char *new_lineptr;
    char c;
    size_t pos = 0;

    if (*lineptr == NULL || *n == 0) {
        *n = 120;
        new_lineptr = (char *) realloc(*lineptr, *n);
        if (new_lineptr == NULL) {
            return -1;
        }
        *lineptr = new_lineptr;
    }
    for (;;) {
        c = fgetc(stream);

        if (c == EOF) {
            break;
        }
        // not enough space for next char + final \0
        if (pos + 2 > *n) {
            new_lineptr = (char *) realloc(*lineptr, *n + 120);
            if (new_lineptr == NULL) {
                return -1;
            }
            *lineptr = new_lineptr;
            *n = *n + 120;
        }

        (*lineptr)[pos] = c;
        pos++;

        if (c == '\n') {
            break;
        }
    }

    if (pos == 0)
        return -1;

    (*lineptr)[pos] = '\0';
    return pos;
}

/*
 * Tun device configuration, post up/down program execution
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
 */

#include "campagnol.h"

#include <arpa/inet.h>
#include <sys/wait.h>

#include "tun_device.h"
#include "configuration.h"
#include "../common/log.h"

/* "itoa"
 * return a newly allocated string
 */
static char *int_to_str(int i) {
    int out_len = 5;
    int r;
    char *out = CHECK_ALLOC_FATAL(malloc(out_len));

    r = snprintf(out, out_len, "%d", i);
    if (r >= out_len) {
        free(out);
        out_len = r+1;
        out = CHECK_ALLOC_FATAL(malloc(out_len));
        r = snprintf(out, out_len, "%d", i);
    }
    if (r == -1) {
        return NULL;
    }
    return out;
}

/* Replace the special variables %D %V ... in s
 * return a newly allocated string
 */
static char *replace_args(const char *s, char *device) {
    char * out;
    size_t len, len_written = 0;
    const char *src;
    char *dst;
    char *tmp;
    size_t tmp_len;
    char buf[3];
    int must_free_tmp = 0;

    buf[0] = '%';
    buf[2] = '\0';

    len = strlen(s)+1;
    out = CHECK_ALLOC_FATAL(malloc(len));

    src = s;
    dst = out;

    while(*src) {
        switch(*src) {
            case '%':
                tmp = NULL;
                switch(*(src+1)) {
                    case '%':
                        must_free_tmp = 1;
                        tmp = CHECK_ALLOC_FATAL(strdup("%"));
                        break;
                    case 'D': // device
                        tmp = device;
                        break;
                    case 'V': // VPN IP
                        tmp = inet_ntoa (config.vpnIP);
                        break;
                    case 'M': // MTU
                        must_free_tmp = 1;
                        tmp = int_to_str(config.tun_mtu);
                        break;
                    case 'N': // netmask
                        tmp = config.network;
                        break;
                    case 'P': // local UDP port
                        must_free_tmp = 1;
                        tmp = int_to_str(config.localport);
                        break;
                    case 'I': // local IP
                        tmp = inet_ntoa(config.localIP);
                        break;
                    default:
                        buf[1] = *(src+1);
                        tmp = buf;
                        break;
                }
                if (tmp != NULL) {
                    tmp_len = strlen(tmp);
                    len += tmp_len - 2; // add replacement and remove %.
                    out = CHECK_ALLOC_FATAL(realloc(out, len));
                    dst = out + len_written;
                    memcpy(dst, tmp, tmp_len);
                    if (must_free_tmp) {
                        free(tmp);
                        must_free_tmp = 0;
                    }
                    dst += tmp_len;
                    len_written += tmp_len;
                    src += 2;
                }
                break;
            default:
                *dst = *src;
                dst++;
                src++;
                len_written++;
                break;
        }
    }

    *dst = '\0';
    len_written++;

    ASSERT(len == len_written);

    return out;
}

/* execute the commands in progs or if default_progs if progs is NULL
 */
static void exec_internal(char **progs, const char ** default_progs, char *device) {
    int r;
    const char **programs = progs != NULL ? (const char **) progs : default_progs;
    char *cmd;
    if (programs != NULL) {
        while (*programs) {
            cmd = replace_args(*programs, device);
            if (config.debug) {
                printf("Running: %s\n", cmd);
            }
            r = system(cmd);
            if (config.debug) {
                printf("Exited with status %d\n", WEXITSTATUS(r));
            }
            free(cmd);
            programs++;
        }
    }
}

void exec_up(char *device) {
    exec_internal(config.exec_up, tun_default_up, device);
}

void exec_down(char *device) {
    exec_internal(config.exec_down, tun_default_down, device);
}

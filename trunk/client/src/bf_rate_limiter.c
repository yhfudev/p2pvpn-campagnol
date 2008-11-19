/*
 * OpenSSL rate limiter filter BIO
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

/* filter BIO
 * use two rate limiters in the writting function.
 * use one for the DTLS channel and one global for the client
 * the rate limiters may be NULL
 *
 * The structure of this file comes from OpenSSL's null filter.
 */

#include "config.h"

#include <stdlib.h>
#include <openssl/err.h>

#include "bf_rate_limiter.h"

static int ratef_write(BIO *h, const char *buf, int num);
static int ratef_read(BIO *h, char *buf, int size);
static long ratef_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int ratef_new(BIO *h);
static int ratef_free(BIO *h);

static BIO_METHOD methods_ratef = {
        BIO_TYPE_RATE_FILTER,
        "Rate limiter filter",
        ratef_write, // write function, use the two rate limiters
        ratef_read, // read function, transparent
        NULL,
        NULL,
        ratef_ctrl,
        ratef_new,
        ratef_free,
        NULL
};

/* global and client: two different rate limiters */
BIO * BIO_f_new_rate_limiter(struct tb_state* global, struct tb_state* client) {
    BIO *bi;
    struct rate_limiter_data *data;
    bi = BIO_new(&methods_ratef);
    if (bi == NULL) {
        return NULL;
    }
    data = malloc(sizeof(struct rate_limiter_data));
    if (data == NULL) {
        BIO_free(bi);
        return NULL;
    }
    data->client = client;
    data->global = global;
    bi->ptr = data;
    bi->init = 1;
    return bi;
}

static int ratef_new(BIO *bi) {
    bi->init = 0;
    bi->ptr = NULL;
    bi->flags = 0;
    return 1;
}

static int ratef_free(BIO *bi) {
    if (bi == NULL) return 0;
    free(bi->ptr);
    return 1;
}

static int ratef_read(BIO *b, char *out, int outl) {
    int ret = 0;

    if (out == NULL) return 0;
    if (b->next_bio == NULL) return 0;
    ret = BIO_read(b->next_bio, out, outl);
    BIO_clear_retry_flags(b);
    BIO_copy_next_retry(b);
    return ret;
}

static int ratef_write(BIO *b, const char *in, int inl) {
    int ret = 0;
    struct rate_limiter_data *data = (struct rate_limiter_data *) b->ptr;

    if ((in == NULL) || inl <=0) return 0;
    if (b->next_bio == NULL) return 0;

    if (data->client != NULL)
        tb_count(data->client, inl);
    if (data->global != NULL)
        tb_count(data->global, inl);
    ret = BIO_write(b->next_bio, in, inl);
    BIO_clear_retry_flags(b);
    BIO_copy_next_retry(b);
    return ret;
}

static long ratef_ctrl(BIO *b, int cmd, long num, void *ptr) {
    long ret = 1;

    if (b->next_bio == NULL) return 0;

    switch(cmd) {
        case BIO_C_DO_STATE_MACHINE:
            BIO_clear_retry_flags(b);
            ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
            BIO_copy_next_retry(b);
            break;
        case BIO_CTRL_DUP:
            ret = 0L;
            break;
        default:
            ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
    }
    return ret;
}

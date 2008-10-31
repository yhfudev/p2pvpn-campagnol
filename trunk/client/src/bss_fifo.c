/*
 * OpenSSL BIO FIFO
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

#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "log.h"
#include "bss_fifo.h"
#include "sem_wrap.h"

/*
 * Internal functions
 */
static int fifo_write(BIO *h, const char *buf, int num);
static int fifo_read(BIO *h, char *buf, int size);
static long fifo_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int fifo_new(BIO *h);
static int fifo_free(BIO *data);
int fifo_allocate(BIO *bi, int len, int data_size);

/* BIO_METHOD structure describing the BIO */
static BIO_METHOD fifo_method = { BIO_TYPE_FIFO, // type
        "fifo buffer", // name
        fifo_write, // write function
        fifo_read, // read function
        NULL, // puts function, not implemented
        NULL, // gets function, not implemented
        fifo_ctrl, // BIO control function
        fifo_new, // creation
        fifo_free, // free
        NULL, // callback function, not used
        };

/*
 * Create a new FIFO BIO.
 * len: number of items in the queue
 * data_size: packet size
 */
BIO *BIO_new_fifo(int len, int data_size) {
    BIO *bi;
    int r;
    bi = BIO_new(&fifo_method);
    if (bi == NULL) {
        return NULL;
    }
    r = fifo_allocate(bi, len, data_size);
    if (r != 1) {
        BIO_free(bi);
        return NULL;
    }
    return bi;
}

/*
 * Allocate everything in the BIO and set bi->init = 1
 * len: number of items in the queue
 * data_size: packet size
 */
int fifo_allocate(BIO *bi, int len, int data_size) {
    int i, j;
    struct fifo_data * d;

    bi->ptr = malloc(sizeof(struct fifo_data));
    if (bi->ptr == NULL) {
        log_error("Cannot allocate a new client");
        return 0;
    }
    d = (struct fifo_data *) bi->ptr;
    d->index_read = 0;
    d->index_write = 0;
    d->size = len;

    d->fifo = (struct fifo_item *) malloc(d->size * sizeof(struct fifo_item));
    if (d->fifo == NULL) {
        free(bi->ptr);
        log_error("Cannot allocate a new client");
        return 0;
    }

    for (i = 0; i < d->size; i++) {
        d->fifo[i].data = malloc(data_size);
        if (d->fifo[i].data == NULL) {
            break;
        }
    }
    if (i != d->size) {
        for (j = 0; j < i; j++) {
            free(d->fifo[j].data);
        }
        free(d->fifo);
        free(bi->ptr);
        log_error("Cannot allocate a new client");
        return 0;
    }

    semInit(&d->sem_read, 0, 0);
    semInit(&d->sem_write, 0, d->size);

    bi->init = 1;

    return 1;
}

/*
 * Doesn't do much
 * the work is done by fifo_allocate
 */
static int fifo_new(BIO *bi) {
    bi->shutdown = 1; // the "close flag" (see BIO_set_close(3))
    bi->init = 0;
    bi->num = -1; // not used

    return 1;
}

/*
 * free fifo_data and fifo_item structures
 */
static int fifo_free(BIO *bi) {
    struct fifo_data *d;
    int i;
    if (bi == NULL)
        return 0; // we have to check
    if (bi->shutdown) {
        if ((bi->init) && (bi->ptr != NULL)) {
            d = (struct fifo_data *) bi->ptr;
            for (i = 0; i < d->size; i++) {
                free(d->fifo[i].data);
            }
            free(d->fifo);
            semDestroy(&d->sem_read);
            semDestroy(&d->sem_write);
            free(d);
            bi->ptr = NULL;
        }
    }
    return 1;
}

/*
 * Blocking read from the FIFO
 */
static int fifo_read(BIO *b, char *out, int outl) {
    int ret = -1, len;
    struct fifo_data *d;
    struct fifo_item *item;

    d = (struct fifo_data *) b->ptr;
    semWait(&d->sem_read);
    BIO_clear_retry_flags(b);

    item = &d->fifo[d->index_read];
    len = item->size;
    ret = (outl >= len) ? len : outl; // is "out" big enough to store the packet
    if ((out != NULL)) {
        memcpy(out, item->data, ret); // copy the data into "out" and update the queue
    }
    (d->index_read == d->size - 1) ? d->index_read = 0 : d->index_read++;
    semPost(&d->sem_write);
    return ret;
}

/*
 * Blocking write to the FIFO
 */
static int fifo_write(BIO *b, const char *in, int inl) {
    struct fifo_data *d;
    struct fifo_item *item;
    int ret = -1;

    if (in == NULL) {
        BIOerr(BIO_F_BIO_WRITE,BIO_R_NULL_PARAMETER);
        return ret;
    }

    d = (struct fifo_data *) b->ptr;
    semWait(&d->sem_write);
    BIO_clear_retry_flags(b);
    item = &d->fifo[d->index_write];
    item->size = inl;
    memcpy(item->data, in, inl);
    (d->index_write == d->size - 1) ? d->index_write = 0 : d->index_write++;
    ret = inl;
    semPost(&d->sem_read);
    return ret;
}

/*
 * All the controls options are not implemented
 */
static long fifo_ctrl(BIO *b, int cmd, long num, void *ptr) {
    long ret = 1;
    int i, v;
    struct fifo_item *item;

    struct fifo_data * d = (struct fifo_data *) b->ptr;

    switch (cmd) {
        case BIO_CTRL_RESET:
            d->index_read = 0;
            d->index_write = 0;
            semDestroy(&d->sem_read);
            semDestroy(&d->sem_write);
            semInit(&d->sem_read, 0, 0);
            semInit(&d->sem_write, 0, d->size);
            break;
        case BIO_CTRL_EOF:
            semGetValue(&d->sem_read, &v);
            ret = (long) (v == 0);
            break;
        case BIO_CTRL_GET_CLOSE:
            ret = (long) b->shutdown;
            break;
        case BIO_CTRL_SET_CLOSE:
            b->shutdown = (int) num;
            break;

        case BIO_CTRL_WPENDING:
            ret = 0L;
            break;
        case BIO_CTRL_PENDING:
            ret = 0;
            semGetValue(&d->sem_read, &v);
            for (i = d->index_read; i < d->index_read + v; i++) {
                item = &d->fifo[((i < d->size) ? i : i - d->size)];
                ret += item->size;
            }
            break;
        case BIO_CTRL_DUP:
        case BIO_CTRL_FLUSH:
            ret = 1;
            break;
        case BIO_CTRL_PUSH:
        case BIO_CTRL_POP:
        case BIO_CTRL_DGRAM_GET_SEND_TIMER_EXP:
        case BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP:
        default:
            ret = 0;
            break;
    }
    return ret;
}
/*
 * OpenSSL BIO FIFO
 * 
 * Copyright (C) 2008 Florent Bondoux
 * 
 * This file is part of Campagnol.
 *
 */

/* This file uses parts of OpenSSL's crypto/bio/bss_mem.c */

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <errno.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "campagnol.h"
#include "bss_fifo.h"
#include "pthread_wrap.h"

/*
 * Internal functions
 */
static int fifo_write(BIO *h, const char *buf, int num);
static int fifo_read(BIO *h, char *buf, int size);
static int fifo_puts(BIO *h, const char *str);
static long fifo_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int fifo_new(BIO *h);
static int fifo_free(BIO *data);

/* BIO_METHOD structure describing the BIO */
static BIO_METHOD fifo_method = {
    BIO_TYPE_FIFO,      // type
    "fifo buffer",      // name
    fifo_write,         // write function
    fifo_read,          // read function
    fifo_puts,          // implemented but not used
    NULL,               // gets function, not implemented
    fifo_ctrl,          // BIO control function
    fifo_new,           // creation
    fifo_free,          // free
    NULL,               // callback function, not used
};

/*
 * Return the methods
 * (exported function)
 */
BIO_METHOD *BIO_s_fifo(void) {
    return(&fifo_method);
}

/*
 * Create the structures associated to a new BIO
 * fifo_data and the fifo_item list
 */
static int fifo_new(BIO *bi) {
    bi->shutdown=1;         // the "close flag" (see BIO_set_close(3))
    bi->init=1;
    bi->num= -1;            // not used
    bi->ptr = malloc(sizeof(struct fifo_data));
    struct fifo_data * d = (struct fifo_data *) bi->ptr;
    d->len = 0;
    d->queue = NULL;
    d->first = NULL;
    
    /* Circularly-linked list of length config.FIO_size */
    d->fifo = malloc(sizeof(struct fifo_item));
    struct fifo_item *prec = d->fifo;
    int i;
    for (i=0; i<config.FIFO_size-1; i++) {
        struct fifo_item *new = malloc(sizeof(struct fifo_item));
        prec->next = new;
        prec = new;
    }
    prec->next = d->fifo; // close the loop
    
    conditionInit(&d->cond, NULL);
    mutexInit(&d->mutex, NULL);
    return(1);
}

static int fifo_free (BIO *bi) {
    if (bi == NULL) return(0); // we have to check
    if (bi->shutdown) {
        if ((bi->init) && (bi->ptr != NULL)) {
            struct fifo_data *d = (struct fifo_data *) bi->ptr;
            struct fifo_item *item = d->fifo;
            struct fifo_item *next;
            int i;
            // free the queue
            for (i=0; i<config.FIFO_size; i++) {
                next = item->next;
                free(item);
                item = next;
            }
            conditionDestroy(&d->cond);
            mutexDestroy(&d->mutex);
            free(d);
            bi->ptr = NULL;
        }
    }
    return(1);
}

/* Blocking read from the FIFO
 */
static int fifo_read(BIO *b, char *out, int outl) {
    int ret = -1, len;
    struct fifo_data *d;
    struct fifo_item *item;
    
    d = (struct fifo_data *) b->ptr;
    mutexLock(&d->mutex);
    BIO_clear_retry_flags(b);
    if (d->len == 0) { // the queue is empty, wait
        d->waiting = 1;
        conditionWait(&d->cond, &d->mutex);
    }
    
    // now it's not empty anymore
    item = d->first;
    len = item->size;
    ret=(outl >= len)?len:outl; // is "out" big enough to store the packet
    if ((out != NULL)) {
        memcpy(out,item->data,ret); // copy the data into "out" and update the queue
        d->len --;
        d->first = item->next;
        if (d->waiting) {
            // unlock a thread that could be waiting in fifo_write
            d->waiting = 0;
            conditionSignal(&d->cond);
        }
    }
    mutexUnlock(&d->mutex);
    return(ret);
}

/*
 * Blocking write to the FIFO
 */
static int fifo_write(BIO *b, const char *in, int inl) {
    struct fifo_data *d;
    struct fifo_item *item;
    int ret = -1;
        
    if (in == NULL) {
        BIOerr(BIO_F_MEM_WRITE,BIO_R_NULL_PARAMETER);
        goto end;
    }
    
    d = (struct fifo_data *) b->ptr;
    
    mutexLock(&d->mutex);
    if (d->len == config.FIFO_size) {
        d->waiting = 1;
        conditionWait(&d->cond, &d->mutex);
    }
    
    BIO_clear_retry_flags(b);
    
    if (d->len == 0) { // empty FIFO
        item = d->fifo; // take d->fifo for the first element
        d->first = item;
    }
    else {
        item = d->queue->next; // otherwise take the next element
    }
    item->size = inl;
    memcpy(item->data, in, inl);
    d->queue = item; // new last element
    ret = inl;
    d->len ++;
    if (d->waiting) {
        d->waiting = 0;
        conditionSignal(&d->cond);
    }
    mutexUnlock(&d->mutex);
end:
    return(ret);

}

/*
 * Large parts of this functions come from OpenSSL's BIO_MEM
 * All the controls options are not implemented
 */
static long fifo_ctrl(BIO *b, int cmd, long num, void *ptr)
    {
    long ret=1;
    
    struct fifo_data * d = (struct fifo_data *) b->ptr;

    switch (cmd)
        {
    case BIO_CTRL_RESET:
        d->first = NULL;
        d->queue = NULL;
        d->len = 0;
        break;
    case BIO_CTRL_EOF:
        ret=(long)(d->len == 0);
        break;
    case BIO_C_SET_BUF_MEM_EOF_RETURN:
        b->num=(int)num;
        break;
    case BIO_CTRL_GET_CLOSE:
        ret=(long)b->shutdown;
        break;
    case BIO_CTRL_SET_CLOSE:
        b->shutdown=(int)num;
        break;

    case BIO_CTRL_WPENDING:
        ret=0L;
        break;
    case BIO_CTRL_PENDING:
        ret = 0;
        struct fifo_item *item = d->fifo;
        while (item != NULL) {
            ret += item->size;
            item = item->next;
        }
        break;
    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
        ret=1;
        break;
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
    default:
        ret=0;
        break;
        }
    return(ret);
    }

    
static int fifo_puts(BIO *bp, const char *str)
    {
    int n,ret;

    n=strlen(str);
    ret=fifo_write(bp,str,n);
    /* memory semantics is that it will always work */
    return(ret);
    }

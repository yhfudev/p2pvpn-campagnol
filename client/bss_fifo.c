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

/* fonctions internes
 */
static int fifo_write(BIO *h, const char *buf, int num);
static int fifo_read(BIO *h, char *buf, int size);
static int fifo_puts(BIO *h, const char *str);
static long fifo_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int fifo_new(BIO *h);
static int fifo_free(BIO *data);

/* Méthodes associées au BIO
 */
static BIO_METHOD fifo_method = {
    BIO_TYPE_FIFO,      // type
    "fifo buffer",      // nom
    fifo_write,         // méthode d'écriture
    fifo_read,          // méthode de lecture
    fifo_puts,          // implantée, mais non utilisé
    NULL,               // méthode gets, non implantée
    fifo_ctrl,          // contrôle du BIO
    fifo_new,           // création
    fifo_free,          // destruction
    NULL,               // Callback, non utilisé
};

/* Retourne les méthodes
 * (fonction exportée)
 */
BIO_METHOD *BIO_s_fifo(void) {
    return(&fifo_method);
}

/* Allocation des structures associées au BIO
 * fifo_data + liste de fifo_item
 */
static int fifo_new(BIO *bi) {
    bi->shutdown=1;         // le "close flag" (voir BIO_set_close(3))
    bi->init=1;
    bi->num= -1;            // non utilisé
    bi->ptr = malloc(sizeof(struct fifo_data));
    struct fifo_data * d = (struct fifo_data *) bi->ptr;
    d->len = 0;
    d->queue = NULL;
    d->first = NULL;
    
    // Pile de longueur BIO_FIFO_LENGTH
    // Éléments chaînés en boucle
    d->fifo = malloc(sizeof(struct fifo_item));
    struct fifo_item *prec = d->fifo;
    int i;
    for (i=0; i<config.FIFO_size-1; i++) {
        struct fifo_item *new = malloc(sizeof(struct fifo_item));
        prec->next = new;
        prec = new;
    }
    prec->next = d->fifo; // fermeture de la boucle
    
    d->cond = createCondition();
    d->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    return(1);
}

static int fifo_free (BIO *bi) {
    if (bi == NULL) return(0); // verifications d'usage
    if (bi->shutdown) {
        if ((bi->init) && (bi->ptr != NULL)) {
            struct fifo_data *d = (struct fifo_data *) bi->ptr;
            struct fifo_item *item = d->fifo;
            struct fifo_item *next;
            int i;
            // destruction de la fifo
            for (i=0; i<config.FIFO_size; i++) {
                next = item->next;
                free(item);
                item = next;
            }
            destroyCondition(&d->cond);
            free(d);
            bi->ptr = NULL;
        }
    }
    return(1);
}

/* Lecture bloquante de la FIFO
 */
static int fifo_read(BIO *b, char *out, int outl) {
    int ret = -1, len;
    struct fifo_data *d;
    struct fifo_item *item;
    
    d = (struct fifo_data *) b->ptr;
    mutexLock(&d->mutex);
    BIO_clear_retry_flags(b);
    if (d->len == 0) { // pile vide : attente
        d->waiting = 1;
        conditionWait(&d->cond, &d->mutex);
    }
    
    // pile non vide maintenant
    item = d->first;
    len = item->size;
    ret=(outl >= len)?len:outl; // buffer assez grand pour accueuillir le paquet ?
    if ((out != NULL)) {
        memcpy(out,item->data,ret); // copie dans out, puis décalage dans la liste chaînée
        d->len --;
        d->first = item->next;
        if (d->waiting) {
            d->waiting = 0;
            conditionSignal(&d->cond);
        }
    }
    mutexUnlock(&d->mutex);
    return(ret);
}

/* Écriture sur la FIFO, 
 * bloque si la FIFO est pleine
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
    
    if (d->len == 0) { // fifo vide, 
        item = d->fifo; // on prend fifo pour le premier élément
        d->first = item;
    }
    else {
        item = d->queue->next; // sinon, on prend le suivant du dernier
    }
    item->size = inl;
    memcpy(item->data, in, inl);
    d->queue = item; // nouveau dernier
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

// Adapté du BIO_MEM d'OpenSSL
// Tous les contrôles ne sont pas implantés
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

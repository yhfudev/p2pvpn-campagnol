/*
 * OpenSSL BIO FIFO
 *
 * Copyright (C) 2008-2009 Florent Bondoux
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


#ifndef BSS_FIFO_H_
#define BSS_FIFO_H_

#include <pthread.h>
#include <openssl/bio.h>

/* BIO type: source/sink */
#define BIO_TYPE_FIFO (23|BIO_TYPE_SOURCE_SINK)

/* controls:
 * DTLS assumes that it is used with a UDP socket BIO
 * so we reuse the same values */

/* get and set the reception timeout
 * the parameter is a struc timespec*
 */
#define BIO_CTRL_FIFO_SET_RECV_TIMEOUT      BIO_CTRL_DGRAM_SET_RECV_TIMEOUT
#define BIO_CTRL_FIFO_GET_RECV_TIMEOUT      BIO_CTRL_DGRAM_GET_RECV_TIMEOUT

/* last operation timed out ?*/
#define BIO_CTRL_FIFO_GET_RECV_TIMER_EXP    BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP
#define BIO_CTRL_FIFO_GET_SEND_TIMER_EXP    BIO_CTRL_DGRAM_GET_SEND_TIMER_EXP

/* discard packets when the FIFO is full instead of waiting */
#define BIO_CTRL_FIFO_SET_DROPTAIL          100
#define BIO_CTRL_FIFO_GET_DROPTAIL          101

/* Create a new BIO */
extern BIO *BIO_new_fifo(int len, int data_size);

/* Data structure used by the BIO */
struct fifo_data {
    unsigned int size;              // Size of the FIFO queue
    unsigned int threshold;         // Threshold to wake up the writing thread
    struct fifo_item *fifo;         // The FIFO's items
    unsigned int index_read;        // Read position
    unsigned int index_write;       // Write position
    pthread_cond_t cond_read;       // Condition variable used to wait before reading
    pthread_cond_t cond_write;      // Condition variable used to wait before writing
    pthread_mutex_t mutex;          // Mutex used with the condition
    unsigned int nelem;             // Number of items written in the FIFO
    int waiting_read;               // Number of threads waiting on cond
    int waiting_write;
    long int rcv_timeout_nsec;      // recv timeout, ns
    time_t rcv_timeout_sec;         // recv timeout, seconds
    int rcv_timer_exp;              // Timeout during fifo_read
    int droptail;                   // Drop new packets when the fifo is full
};

/* An item in the queue */
struct fifo_item {
    int size;                       // Size of the packet
    char *data;                     // Contains the data
};

#endif /*BSS_FIFO_H_*/

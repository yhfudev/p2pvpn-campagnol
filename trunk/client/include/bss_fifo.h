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


#ifndef BSS_FIFO_H_
#define BSS_FIFO_H_

#include "communication.h"

/* BIO type: source/sink */
#define BIO_TYPE_FIFO (23|BIO_TYPE_SOURCE_SINK)

/* Create a new BIO */
extern BIO_METHOD *BIO_s_fifo(void);

/* Data structure used by the BIO */
struct fifo_data {
    struct fifo_item *fifo;         // One item in the list
    struct fifo_item *first;        // First item in the queue if it is not empty
    struct fifo_item *queue;        // Last item in the queue if it is not empty
    int len;                        // Size of the FIFO queue
    pthread_cond_t cond;            // Pthread condition used to create a blocking BIO
    pthread_mutex_t mutex;          // Mutex used to protect the queue
    int waiting;                    // The FIFO is locked during an I/O operation
};

/* An item in the queue */
struct fifo_item {
    int size;                       // Size of the packet
    char *data;                     // Contains the data
    struct fifo_item *next;
};

#endif /*BSS_FIFO_H_*/

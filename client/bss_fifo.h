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

// Type du BIO
#define BIO_TYPE_FIFO (23|BIO_TYPE_SOURCE_SINK)

// création du BIO
extern BIO_METHOD *BIO_s_fifo(void);

// structure de donnée du BIO
struct fifo_data {
    struct fifo_item *fifo;         // Un élément de la pile
    struct fifo_item *first;        // Premier élément de la pile si non vide
    struct fifo_item *queue;        // Dernier élément de la pile si non vide
    int len;                        // Nombre d'éléments dans la pile
    pthread_cond_t cond;            // Condition pour accès bloquant
    pthread_mutex_t mutex;          // Mutex d'accès
    int waiting;                    // Bloqué en lecture ou écriture
};

// élément de la pile
struct fifo_item {
    int size;                       // taille du paquet
    char data[2000];                // Voir comment la découpe des paquets se fait. ssl->mtu ?
    struct fifo_item *next;
};

#endif /*BSS_FIFO_H_*/

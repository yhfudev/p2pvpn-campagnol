/*
 * tdestroy replacement
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

#include "config.h"
#include <stdlib.h>
#include <search.h>

#include "tdestroy.h"

/* POSIX doesn't let us know about the node structure
 * so we simply use tdelete.
 * It's not efficient and we have to know the comparison routine
 */
void campagnol_tdestroy(void *root, void(*free_node)(void *nodep),
        int(*compar)(const void *, const void *)) {
    void *node;
    while (root != NULL) {
        node = *(void **) root;
        tdelete(node, &root, compar);
        free_node(node);
    }
}

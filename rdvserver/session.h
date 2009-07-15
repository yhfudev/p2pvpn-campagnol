/*
 * Session list definition
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

#ifndef SESSION_H_
#define SESSION_H_

#include "peer.h"

struct session {
    struct client *peer1;
    struct client *peer2;
    time_t time;
    struct session *next;
    struct session *prev;
};

extern struct session *sessions;

extern struct session * add_session(struct client *peer1, struct client *peer2, time_t t);
extern void remove_session(struct session *s);
extern struct session * get_session(struct client *peer1, struct client *peer2);
extern void remove_sessions_with_client(struct client *peer);

#define session_update_time(s)  ({\
    (s)->time = time(NULL);\
    })

#endif /* SESSION_H_ */

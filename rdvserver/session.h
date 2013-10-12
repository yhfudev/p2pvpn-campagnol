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
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * 
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
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

/*
 * Campagnol RDV server, session management
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

#include "rdv.h"
#include "../common/log.h"
#include "session.h"

#include <search.h>

/* linked list of the sessions */
struct session *sessions = NULL;

/* tree storing the session */
static void *sessions_root = NULL;

/* comparison function for the tree */
static int compare_sessions(const void *itema, const void *itemb) {
    const struct session *sess1 = (const struct session *)itema;
    const struct session *sess2 = (const struct session *)itemb;
    if (sess1->peer1 < sess2->peer1) {
        return -1;
    }
    if (sess1->peer1 == sess2->peer1) {
        if (sess1->peer2 < sess2->peer2) {
            return -1;
        }
        if (sess1->peer2 == sess2->peer2) {
            return 0;
        }
        return 1;
    }
    return 1;
}

struct session * add_session(struct client *peer1, struct client *peer2, time_t t) {
    void *slot;
    struct session *sess = malloc(sizeof(struct session));
    if (sess == NULL) {
        log_error(errno, "Cannot allocate a new session (malloc)");
        return NULL;
    }

    sess->time = t;
    sess->peer1 = peer1;
    sess->peer2 = peer2;

    slot = tsearch((void *) sess, &sessions_root, compare_sessions);
    if (slot == NULL) {
        log_error(errno, "Cannot allocate a new session (tsearch)");
        free(sess);
        return NULL;
    }

    log_message_level(1, "New connection from %s to %s", peer1->vpnIP_string,
            peer2->vpnIP_string);

    sess->next = sessions;
    sess->prev = NULL;
    if (sess->next) sess->next->prev = sess;
    sessions = sess;

    return sess;
}

void remove_session(struct session *s) {
    log_message_level(1, "Remove connection from %s to %s",
            s->peer1->vpnIP_string, s->peer2->vpnIP_string);
    if (s->next) s->next->prev = s->prev;
    if (s->prev) {
        s->prev->next = s->next;
    }
    else {
        sessions = s->next;
    }

    tdelete(s, &sessions_root, compare_sessions);

    free(s);
}

struct session * get_session(struct client *peer1, struct client *peer2) {
    struct session sess_tmp, *sess;
    sess_tmp.peer1 = peer1;
    sess_tmp.peer2 = peer2;
    sess = tfind(&sess_tmp, &sessions_root, compare_sessions);
    if (sess != NULL)
        sess = *(void **)sess;
    return sess;
}

void remove_sessions_with_client(struct client *peer) {
    struct session *sess = sessions;
    struct session *next;
    while (sess != NULL) {
        next = sess->next;
        if (sess->peer1 == peer || sess->peer2 == peer) {
            remove_session(sess);
        }
        sess = next;
    }
}

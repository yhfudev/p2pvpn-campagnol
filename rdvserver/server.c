/*
 * Campagnol RDV server, main code
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
#include "server.h"
#include "peer.h"
#include "session.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#define init_timeout(timeout) ({ \
    (timeout)->tv_sec = SELECT_DELAY_SEC; \
    (timeout)->tv_usec = SELECT_DELAY_USEC; \
    })

#define IN_ADDR_EMPTY ((struct in_addr) { 0 })

static inline ssize_t
        send_message(unsigned char type, uint16_t port, struct in_addr ip1,
                struct in_addr ip2, int sockfd, struct sockaddr *to);
static inline void send_OK(struct sockaddr *to, int sockfd);
static inline void send_NOK(struct sockaddr *to, int sockfd);
static inline void send_REJECT(struct in_addr ip, struct sockaddr *to,
        int sockfd);
static inline void send_PONG(struct sockaddr *to, int sockfd);
static inline void send_RECONNECT(struct sockaddr *to, int sockfd);
static inline void send_ANS(struct client *client,
        struct client *requested_client, int send_local, int sockfd);
static inline void send_FWD(struct client *client,
        struct client *requested_client, int send_local, int sockfd);

static void clean_dead_clients(void);

static const char * type_to_string(unsigned char type) {
    switch (type) {
        case HELLO:
            return "HELLO";
            break;
        case PING:
            return "PING";
            break;
        case ASK_CONNECTION:
            return "ASK_CONNECTION";
            break;
        case PONG:
            return "PONG";
            break;
        case OK:
            return "OK";
            break;
        case NOK:
            return "NOK";
            break;
        case FWD_CONNECTION:
            return "FWD_CONNECTION";
            break;
        case ANS_CONNECTION:
            return "ANS_CONNECTION";
            break;
        case REJ_CONNECTION:
            return "REJ_CONNECTION";
            break;
        case PUNCH:
            return "PUNCH";
            break;
        case BYE:
            return "BYE";
            break;
        case RECONNECT:
            return "RECONNECT";
            break;
        case CLOSE_CONNECTION:
            return "CLOSE_CONNECTION";
            break;
        default:
            return "UNKNOWN MESSAGE";
            break;
    }
}

static void dump_message(message_t *msg) {
    const char *type;
    char *tmp_ip;
    puts("-----------------------------------------------------");
    switch (msg->type) {
        case HELLO:
            type = "HELLO";
            break;
        case PING:
            type = "PING";
            break;
        case ASK_CONNECTION:
            type = "ASK";
            break;
        case PONG:
            type = "PONG";
            break;
        case OK:
            type = "OK";
            break;
        case NOK:
            type = "NOK";
            break;
        case FWD_CONNECTION:
            type = "FWD";
            break;
        case ANS_CONNECTION:
            type = "ANS";
            break;
        case REJ_CONNECTION:
            type = "REJ";
            break;
        case PUNCH:
            type = "PUNCH";
            break;
        case PUNCH_KEEP_ALIVE:
            type = "ALIVE";
            break;
        case BYE:
            type = "BYE";
            break;
        case RECONNECT:
            type = "RCNCT";
            break;
        case CLOSE_CONNECTION:
            type = "CLOSE";
            break;
        default:
            type = "?";
            break;
    }

    tmp_ip = CHECK_ALLOC_FATAL(strdup(inet_ntoa(msg->ip1)));
    printf("| %5s | %5d | %15s | %15s |\n", type, ntohs(msg->port), tmp_ip, inet_ntoa(msg->ip2));
    puts("-----------------------------------------------------");
    free(tmp_ip);
}

static void handle_packet(message_t *rmsg, struct sockaddr_in *unknownaddr, int sockfd) {
    struct client *peer, *peer_tmp;
    struct session *sess_tmp, *rev_sess_tmp;
    int send_local_ip;

    peer = get_client_real(unknownaddr);

    if (config.debug) {
        if (peer == NULL) {
            printf("<< %s received from %s:%d\n", type_to_string(rmsg->type),
                    inet_ntoa(unknownaddr->sin_addr), ntohs(unknownaddr->sin_port));
        }
        else {
            printf("<< %s received from %s:%d (%s)\n", type_to_string(
                    rmsg->type), inet_ntoa(unknownaddr->sin_addr), ntohs(unknownaddr->sin_port),
            peer->vpnIP_string);
        }
    }
    if (config.dump) {
        dump_message(rmsg);
    }

    if (peer == NULL && rmsg->type != HELLO) {
        send_RECONNECT((struct sockaddr *) unknownaddr, sockfd);
        return;
    }

    switch (rmsg->type) {
        case HELLO:
            if (peer == NULL) {
                peer_tmp = get_client_VPN(&rmsg->ip1);
                if (peer_tmp != NULL) {
                    if (client_is_timeout(peer_tmp)) {
                        remove_sessions_with_client(peer_tmp);
                        remove_client(peer_tmp);
                    }
                    else {
                        send_NOK((struct sockaddr *) unknownaddr, sockfd);
                        break;
                    }
                }

                if (config.max_clients != 0 && n_clients == config.max_clients) {
                    clean_dead_clients();
                    if (n_clients == config.max_clients) {
                        send_NOK((struct sockaddr *) unknownaddr, sockfd);
                        break;
                    }
                }

                if (rmsg->port != 0) {
                    peer = add_client(sockfd, time(NULL),
                            unknownaddr->sin_addr, unknownaddr->sin_port,
                            rmsg->ip1, rmsg->ip2, rmsg->port);
                }
                else {
                    peer = add_client(sockfd, time(NULL),
                            unknownaddr->sin_addr, unknownaddr->sin_port,
                            rmsg->ip1, IN_ADDR_EMPTY, 0);
                }

                if (peer != NULL) {
                    send_OK((struct sockaddr *) unknownaddr, sockfd);
                }

            }
            else {
                if (client_is_timeout(peer)) {
                    if (peer->vpnIP.s_addr == rmsg->ip1.s_addr) {
                        client_update_time(peer);
                        send_OK((struct sockaddr *) unknownaddr, sockfd);
                    }
                    else {
                        remove_sessions_with_client(peer);
                        remove_client(peer);
                        if (rmsg->port != 0) {
                            peer = add_client(sockfd, time(NULL),
                                    unknownaddr->sin_addr,
                                    unknownaddr->sin_port, rmsg->ip1,
                                    rmsg->ip2, rmsg->port);
                        }
                        else {
                            peer = add_client(sockfd, time(NULL),
                                    unknownaddr->sin_addr,
                                    unknownaddr->sin_port, rmsg->ip1,
                                    IN_ADDR_EMPTY, 0);
                        }
                        if (peer != NULL) {
                            send_OK((struct sockaddr *) unknownaddr, sockfd);
                        }
                    }
                }
                else {
                    send_NOK((struct sockaddr *) unknownaddr, sockfd);
                }
            }

            break;
        case BYE:
            remove_sessions_with_client(peer);
            remove_client(peer);
            break;
        case PING:
            send_PONG((struct sockaddr *) unknownaddr, sockfd);
            client_update_time(peer);
            break;
        case ASK_CONNECTION:
            peer_tmp = get_client_VPN(&rmsg->ip1);
            if (peer_tmp == NULL) {
                send_REJECT(rmsg->ip1, (struct sockaddr *) unknownaddr, sockfd);
            }
            else {
                if (client_is_timeout(peer_tmp)) {
                    send_REJECT(rmsg->ip1, (struct sockaddr *) unknownaddr,
                            sockfd);
                    remove_sessions_with_client(peer_tmp);
                    remove_client(peer_tmp);
                    break;
                }

                sess_tmp = get_session(peer, peer_tmp);

                send_local_ip = (peer->localaddr.sin_port != 0)
                        && (peer_tmp->localaddr.sin_port != 0)
                        && (peer->clientaddr.sin_addr.s_addr
                                == peer_tmp->clientaddr.sin_addr.s_addr);

                if (sess_tmp == NULL) {
                    rev_sess_tmp = get_session(peer_tmp, peer);
                    if (rev_sess_tmp != NULL) {
                        remove_session(rev_sess_tmp);
                    }
                    sess_tmp = add_session(peer, peer_tmp, time(NULL));
                    if (sess_tmp != NULL) {
                        send_ANS(peer, peer_tmp, send_local_ip, sockfd);
                        send_FWD(peer_tmp, peer, send_local_ip, sockfd);
                    }
                    else {
                        send_REJECT(rmsg->ip1, (struct sockaddr *) unknownaddr,
                                sockfd);
                    }
                }
                else {
                    send_ANS(sess_tmp->peer1, sess_tmp->peer2, send_local_ip,
                            sockfd);
                    send_FWD(sess_tmp->peer2, sess_tmp->peer1, send_local_ip,
                            sockfd);
                    session_update_time(sess_tmp);
                }
            }
            break;
        case CLOSE_CONNECTION:
            peer_tmp = get_client_VPN(&rmsg->ip1);
            if (peer_tmp != NULL) {
                sess_tmp = get_session(peer, peer_tmp);
                if (sess_tmp != NULL) {
                    remove_session(sess_tmp);
                }
                sess_tmp = get_session(peer_tmp, peer);
                if (sess_tmp != NULL) {
                    remove_session(sess_tmp);
                }
            }
            break;
        default:
            break;
    }
}

void rdv_server(int sockfd) {
    int r_select;
    ssize_t r;
    fd_set fd_select;
    struct timeval timeout;
    message_t rmsg;
    struct sockaddr_in unknownaddr;
    socklen_t len = sizeof(struct sockaddr_in);
    time_t last_cleaning = 0, t;
    struct client *peer, *next;

    log_message("Server listening on port %d", config.serverport);

    while (!end_server) {
        FD_ZERO(&fd_select);
        FD_SET(sockfd, &fd_select);

        init_timeout(&timeout);
        r_select = select(sockfd + 1, &fd_select, NULL, NULL, &timeout);

        if (r_select> 0) {
            r = recvfrom(sockfd, (unsigned char *) &rmsg, sizeof(rmsg), 0, (struct sockaddr *) &unknownaddr, &len);
            t = time(NULL);

            if (r == sizeof(message_t)) {
                handle_packet(&rmsg, &unknownaddr, sockfd);
            }

            if (t - last_cleaning> 5) {
                last_cleaning = t;
                clean_dead_clients();
            }
        }
        else if (r_select == 0) {
            t = time(NULL);
            last_cleaning = t;
            clean_dead_clients();
        }
    }

    peer = clients;
    while (peer != NULL) {
        next = peer->next;
        remove_sessions_with_client(peer);
        remove_client(peer);
        peer = next;
    }
}

ssize_t send_message(unsigned char type, uint16_t port, struct in_addr ip1,
        struct in_addr ip2, int sockfd, struct sockaddr *to) {
    ssize_t s;
    message_t smsg;
    smsg.type = type;
    smsg.port = port;
    smsg.ip1 = ip1;
    smsg.ip2 = ip2;

    if (config.debug) {
        printf(">> %s sent to client %s:%d\n", type_to_string(type), inet_ntoa(
                ((struct sockaddr_in *) to)->sin_addr), ntohs(((struct sockaddr_in *) to)->sin_port));
    }
    if (config.dump) {
        dump_message(&smsg);
    }
    s = sendto(sockfd, &smsg, sizeof(smsg), 0, to, sizeof(*to));
    if (s == -1) {
        log_error(errno, "sendto");
        return -1;
    }
    return s;
}

void clean_dead_clients() {
    struct client *peer, *next;
    peer = clients;
    while (peer != NULL) {
        next = peer->next;
        if (client_is_dead(peer)) {
            remove_sessions_with_client(peer);
            remove_client(peer);
        }
        peer = next;
    }
}

void send_OK(struct sockaddr *to, int sockfd) {
    send_message(OK, 0, IN_ADDR_EMPTY, IN_ADDR_EMPTY, sockfd, to);
}

void send_NOK(struct sockaddr *to, int sockfd) {
    send_message(NOK, 0, IN_ADDR_EMPTY, IN_ADDR_EMPTY, sockfd, to);
}

void send_REJECT(struct in_addr ip, struct sockaddr *to, int sockfd) {
    send_message(REJ_CONNECTION, 0, ip, IN_ADDR_EMPTY, sockfd, to);
}

void send_PONG(struct sockaddr *to, int sockfd) {
    send_message(PONG, 0, IN_ADDR_EMPTY, IN_ADDR_EMPTY, sockfd, to);
}

void send_RECONNECT(struct sockaddr *to, int sockfd) {
    send_message(RECONNECT, 0, IN_ADDR_EMPTY, IN_ADDR_EMPTY, sockfd, to);
}

void send_ANS(struct client *client, struct client *requested_client,
        int send_local, int sockfd) {
    if (send_local) {
        send_message(ANS_CONNECTION, requested_client->localaddr.sin_port,
                requested_client->localaddr.sin_addr, requested_client->vpnIP,
                sockfd, (struct sockaddr *) &client->clientaddr);
    }
    else {
        send_message(ANS_CONNECTION, requested_client->clientaddr.sin_port,
                requested_client->clientaddr.sin_addr, requested_client->vpnIP,
                sockfd, (struct sockaddr *) &client->clientaddr);
    }
}

void send_FWD(struct client *client, struct client *requested_client,
        int send_local, int sockfd) {
    if (send_local) {
        send_message(FWD_CONNECTION, requested_client->localaddr.sin_port,
                requested_client->localaddr.sin_addr, requested_client->vpnIP,
                sockfd, (struct sockaddr *) &client->clientaddr);
    }
    else {
        send_message(FWD_CONNECTION, requested_client->clientaddr.sin_port,
                requested_client->clientaddr.sin_addr, requested_client->vpnIP,
                sockfd, (struct sockaddr *) &client->clientaddr);
    }
}

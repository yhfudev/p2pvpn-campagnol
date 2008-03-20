/*
 * Campagnol main code
 * 
 * Copyright (C) 2007 Antoine Vianey
 *               2008 Florent Bondoux
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

#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

/** definitions des constantes utilisees */
// ID des paquets de structure
#define HELLO 0
#define PING 1
#define ASK_CONNECTION 2
#define PONG 3
#define OK 4
#define NOK 5
#define FWD_CONNECTION 6
#define ANS_CONNECTION 7
#define REJ_CONNECTION 8
#define PUNCH 9
#define PUNCH_ACK 10
#define BYE 11
#define RECONNECT 12
#define CLOSE_CONNECTION 13

// duree de blocage du select, frequence des pings au serveur
#define ATTENTE_SECONDES 2
#define ATTENTE_MICROSECONDES 0

#define MAX_REGISTERING_TRIES 4

#define MESSAGE_MAX_LENGTH 1500
#define NOMBRE_DE_PUNCH 5
#define PAUSE_ENTRE_PUNCH 1000000

/** fin des definitions des constantes utilisees */


/** structure d'un message echange avec le server et de controle entre clients */
struct message {
    unsigned char type;             // byte : type du message
    uint16_t port;                // 2 bytes : port
    struct in_addr ip1;           // 4 bytes : address IP 1 (IPv4)
    struct in_addr ip2;           // 4 bytes : address IP 2 (IPv4)
}  __attribute__ ((packed));
/** fin structure */

/** structure argument pour le thread de punch */
struct punch_arg {
    struct client *peer;
    int *sockfd;
};
/** fin de structure */

struct comm_args {
    int sockfd;
    int tunfd;
};

extern int register_rdv(int sockfd);
extern void lancer_vpn(int sockfd, int tunfd);

#endif /*COMMUNICATION_H_*/

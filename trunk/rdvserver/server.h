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

#ifndef SERVER_H_
#define SERVER_H_

/*
 * Message types (1 byte)
 * Must be different from the DTLS content type values
 */
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
#define PUNCH_KEEP_ALIVE 10
#define BYE 11
#define RECONNECT 12
#define CLOSE_CONNECTION 13

/*
 * duration of the timeout used with the select calls*/
#define SELECT_DELAY_SEC 2
#define SELECT_DELAY_USEC 0

/*
 * The structure of the UDP messages between a client and the RDV server
 */
typedef struct {
    unsigned char type;           // 1 byte : message type
    uint16_t port;                // 2 bytes : port
    struct in_addr ip1;           // 4 bytes : IP address 1 (IPv4)
    struct in_addr ip2;           // 4 bytes : IP address 2 (IPv4)
}  __attribute__ ((packed)) // important
message_t;

extern void rdv_server(int sockfd);

#endif /* SERVER_H_ */

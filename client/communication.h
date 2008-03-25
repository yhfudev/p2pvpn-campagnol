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

/* Message types */
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

/*
 * duration of the timeout used with the select calls
 * give the period of the PING messages when the client is inactive*/
#define SELECT_DELAY_SEC 2
#define SELECT_DELAY_USEC 0

/*
 * Number of tries when registering to the rendezvous server
 */
#define MAX_REGISTERING_TRIES 4

/*
 * Max UDP message length
 * the MTU on the TUN device is 1400
 * the overhead is (surely...) less than 100 bytes
 */
#define MESSAGE_MAX_LENGTH 1500
/*
 * Number of punch messages
 */
#define PUNCH_NUMBER 5
/*
 * Duration between two punch messages
 */
#define PUNCH_DELAY_USEC 1000000



/*
 * The structure of the UDP messages between a client and the RDV server
 */
struct message {
    unsigned char type;           // 1 byte : message type
    uint16_t port;                // 2 bytes : port
    struct in_addr ip1;           // 4 bytes : IP address 1 (IPv4)
    struct in_addr ip2;           // 4 bytes : IP address 2 (IPv4)
}  __attribute__ ((packed)); // important

/* arguments for the punch thread */
struct punch_arg {
    struct client *peer;
    int sockfd;
};

/* arguments for the comm_tun and comm_socket threads */
struct comm_args {
    int sockfd;
    int tunfd;
};

extern int register_rdv(int sockfd);
extern void start_vpn(int sockfd, int tunfd);

#endif /*COMMUNICATION_H_*/

/*
 * Campagnol main code
 *
 * Copyright (C) 2007 Antoine Vianey
 *               2008-2009 Florent Bondoux
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

#include "rate_limiter.h"

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
/*DTLS content types */
#define DTLS_CHANGE_CIPHER_SPEC 20
#define DTLS_ALERT 21
#define DTLS_HANDSHAKE 22
#define DTLS_APPLICATION_DATA 23

/*
 * duration of the timeout used with the select calls*/
#define SELECT_DELAY_SEC 2
#define SELECT_DELAY_USEC 0

/*
 * time between two PING messages
 */
#define TIMER_PING_SEC 3
#define TIMER_PING_USEC 0

/*
 * Number of tries when registering to the rendezvous server
 */
#define MAX_REGISTERING_TRIES 4

/*
 * TUN_MTU_DEFAULT: default MTU on the TUN device if not overridden in the
 * configuration file
 *
 * MESSAGE_MAX_LENGTH: Max UDP message length
 * it must be large enough for the maximum data size + the maximum DTLS overhead.
 *
 * the maximum data size is the MTU of the tun interface: TUN_MTU
 * for campagnol, there is no data compression, so no potential compression overhead
 *
 * the DTLS overhead is:
 * record layer + MAC + padding + padding length field + IV
 * - record layer is 13 bytes
 * - MAC is 20 bytes for SHA1, 16 bytes for MD5
 * - the padding length field is 1 byte (so the padding is <= 255 bytes)
 * - the total length of the (compressed) data, the IVs, the MAC, the padding
 *   and the padding length must be a multiple of the cipher's block size.
 *
 * so, for camellia 256 + SHA1, and a packet size of 1400 bytes
 * - the block size is 16 bytes. it's also the IV size.
 * - the MAC size is 20 bytes
 * - need to pad 1400+20+1+16 = 1437 to the next multiple of 16 = 1440
 *   (add 3 bytes of padding)
 * - the total message length is 1440 + 13 = 1453
 *   1400 (data) + 20 (MAC) + 3 (padding) + 1 (padding length) + 16 (IVs) + 13 (record layer)
 *
 * we can also estimate the maximum overhead supposing that the IV size is the same as the block size
 * block size | MAC size | overhead
 *    8 (64)  |    20    |    49
 *   16 (128) |    20    |    65
 *    8 (64)  |    64    |    93    for future sha-512...
 *   16 (128) |    64    |   109
 *
 * AFAIK, DTLS 1.0 only supports MD5 or SHA1 for the MAC, and openssl has no block cipher with block sizes
 * larger than 128 bits.
 * TLS 1.2 will allow other MAC like SHA-512
 *
 * If you want to estimate the best MTU for the VPN according to your
 * network MTU and cipher algorithm, don't forget the IP and UDP headers.
 * The overall overhead for campagnol is:
 * 20 (IP) + 8 (UDP) + DTLS overhead
 *
 */
#define TUN_MTU_DEFAULT 1419
#define MESSAGE_MAX_LENGTH (unsigned int) (config.tun_mtu+200)
/*
 * Number of punch messages
 */
#define PUNCH_NUMBER 10
/*
 * Duration between two punch messages
 */
#define PUNCH_DELAY_USEC 200000


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

/* DTLS header */
typedef struct {
    unsigned char contentType; // same as struct message's type (unsigned char)
    uint16_t version;
    uint16_t epoch;
    uint64_t seq_number:48;
    uint16_t length;
} __attribute__((packed)) dtlsheader_t;

/*
 * Union used to store a VPN packet
 */
typedef union {
    struct ip *ip;
    dtlsheader_t *dtlsheader;
    message_t *message;
    unsigned char *raw;
} packet_t;

/* arguments for the rdv handling thread */
struct rdv_args {
    BIO *fifo;
    int sockfd;
    int tunfd;
};

/* arguments for the punch thread */
struct punch_arg {
    struct client *peer;
    int sockfd;
};

/* arguments for the comm_tun and comm_socket threads */
struct comm_args {
    int sockfd;
    int tunfd;
    struct rdv_args *rdvargs;
};

extern int start_vpn(int sockfd, int tunfd);

/* the handler for SIGALRM */
extern void handler_sigTimerPing(int sig);

/* The rate limiter for the whole client */
extern struct tb_state global_rate_limiter;


/* wrapper around sendto for non blocking I/O */
static inline ssize_t xsendto(int sockfd, const void *buf, size_t len, int flags, const
        struct sockaddr *dest_addr, socklen_t addrlen) {
    ssize_t r;
    fd_set set;

    while ((r = sendto(sockfd, buf, len, flags, dest_addr, addrlen)) == -1
            && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        FD_ZERO(&set);
        FD_SET(sockfd, &set);
        select(sockfd+1, NULL, &set, NULL, NULL);
    }
    return r;
}

#endif /*COMMUNICATION_H_*/

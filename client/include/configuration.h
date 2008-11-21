/*
 * Campagnol configuration
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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <openssl/ssl.h>

struct configuration {
    int verbose;                                // verbose
    int debug;                                  // more verbose
    int daemonize;                              // daemonize the client
    struct in_addr localIP;                     // local IP address
    unsigned int localport;                     // local UDP port
    struct sockaddr_in serverAddr;              // rendezvous server inet address
    struct in_addr vpnIP;                       // VPN IP address
    char *network;                              // VPN subnetwork
    int send_local_addr;                        // Send the local IP/port to the RDV server
    int tun_mtu;                                // MTU of the tun device
    struct in_addr vpnBroadcastIP;              // "broadcast" IP, computed from vpnIP and network
    char *iface;                                // bind to a specific network interface
    char *certificate_pem;                      // PEM formated file containing the client certificate
    char *key_pem;                              // PEM formated file containing the client private key
    char *verif_pem;                            // PEM formated file containing the root certificates
    char *cipher_list;                          // ciphers list for SSL_CTX_set_cipher_list
                                                // see openssl ciphers man page
    X509_CRL *crl;                              // The parsed CRL or NULL
    int FIFO_size;                              // Size of the FIFO list for the incoming packets
    float tb_client_rate;                       // Maximum outgoing rate for the client
    float tb_connection_rate;                   // Maximum outgoing rate for each connection
    size_t tb_client_size;                      // Bucket size for the client
    size_t tb_connection_size;                  // Bucket size for a connection
    int timeout;                                // wait timeout secs before closing a session for inactivity
    int max_clients;                            // maximum number of clients
    char *pidfile;                              // PID file in daemon mode
};

extern void parseConfFile(char *file);
extern void freeConfig(void);

/* names of sections and options */
#define SECTION_NETWORK     "NETWORK"
#define SECTION_VPN         "VPN"
#define SECTION_CLIENT      "CLIENT"
#define SECTION_SECURITY    "SECURITY"

#define SECTION_DEFAULT     "DEFAULT"

#define OPT_LOCAL_HOST      "local_host"
#define OPT_LOCAL_PORT      "local_port"
#define OPT_SERVER_HOST     "server_host"
#define OPT_SERVER_PORT     "server_port"
#define OPT_TUN_MTU         "tun_mtu"
#define OPT_INTERFACE       "interface"
#define OPT_SEND_LOCAL      "use_local_addr"

#define OPT_VPN_IP          "vpn_ip"
#define OPT_VPN_NETWORK     "network"

#define OPT_CERTIFICATE     "certificate"
#define OPT_KEY             "key"
#define OPT_CA              "ca_certificates"
#define OPT_CRL             "crl_file"
#define OPT_CIPHERS         "cipher_list"

#define OPT_FIFO            "fifo_size"
#define OPT_CLIENT_RATE     "client_max_rate"
#define OPT_CONNECTION_RATE "connection_max_rate"
#define OPT_TIMEOUT         "timeout"
#define OPT_MAX_CLIENTS     "max_clients"

#endif /*CONFIGURATION_H_*/

/*
 * Campagnol configuration
 *
 * Copyright (C) 2008-2009 Florent Bondoux
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

struct configuration {
    int verbose;                                // verbose
    int debug;                                  // more verbose
    int daemonize;                              // daemonize the client

    struct in_addr localIP;                     // local IP address
    uint16_t localport;                         // local UDP port
    struct sockaddr_in serverAddr;              // rendezvous server inet address
    struct in_addr vpnIP;                       // VPN IP address
    struct in_addr vpnNetmask;                  // VPN Netmask
    char *network;                              // VPN subnetwork as a string
    int send_local_addr;                        // 1: Send the local IP/port to the RDV server
                                                // 2: Send override_local_addr instead of the real local address
                                                // 0: Do not publish a local address
    struct sockaddr_in override_local_addr;
    int tun_mtu;                                // MTU of the tun device
    struct in_addr vpnBroadcastIP;              // "broadcast" IP, computed from vpnIP and network
    char *iface;                                // bind to a specific network interface
    char *tun_device;                           // The name of the TUN interface
    char *tap_id;                               // Version of the OpenVPN's TAP driver

    char *certificate_pem;                      // PEM formated file containing the client certificate
    char *key_pem;                              // PEM formated file containing the client private key
    char *verif_pem;                            // PEM formated file containing the root certificates
    char *verif_dir;                            // directory containing root certificates
    int verify_depth;                           // Maximum depth for the certificate chain verification
    char *cipher_list;                          // ciphers list for SSL_CTX_set_cipher_list
                                                // see openssl ciphers man page
    char *crl;                                  // A CRL or NULL

    int FIFO_size;                              // Size of the FIFO list for the incoming packets
    float tb_client_rate;                       // Maximum outgoing rate for the client
    float tb_connection_rate;                   // Maximum outgoing rate for each connection
    size_t tb_client_size;                      // Bucket size for the client
    size_t tb_connection_size;                  // Bucket size for a connection
    int timeout;                                // wait timeout secs before closing a session for inactivity
    int max_clients;                            // maximum number of clients
    unsigned int keepalive;                     // seconds between keepalive messages;
    char ** exec_up;                            // UP commands
    char ** exec_down;                          // DOWN commands

    char *pidfile;                              // PID file in daemon mode

#ifdef HAVE_LINUX
    int txqueue;                                // TX queue length for the TUN device (0 means default)
    int tun_one_queue;                          // Single queue mode
#endif
};

extern void initConfig(void);
extern int parseConfFile(const char *file);
extern void freeConfig(void);

/* names of sections and options */
#define SECTION_NETWORK     "NETWORK"
#define SECTION_VPN         "VPN"
#define SECTION_CLIENT      "CLIENT"
#define SECTION_SECURITY    "SECURITY"
#define SECTION_COMMANDS    "COMMANDS"

#define OPT_LOCAL_HOST      "local_host"
#define OPT_LOCAL_PORT      "local_port"
#define OPT_SERVER_HOST     "server_host"
#define OPT_SERVER_PORT     "server_port"
#define OPT_TUN_MTU         "tun_mtu"
#define OPT_INTERFACE       "interface"
#define OPT_SEND_LOCAL      "use_local_addr"
#define OPT_OVRRIDE_LOCAL   "override_local_addr"
#define OPT_TUN_DEVICE      "tun_device"
#define OPT_TAP_ID          "tap_id"

#define OPT_VPN_IP          "vpn_ip"
#define OPT_VPN_NETWORK     "network"

#define OPT_CERTIFICATE     "certificate"
#define OPT_KEY             "key"
#define OPT_CA              "ca_certificates"
#define OPT_CA_DIR          "ca_certificates_dir"
#define OPT_CRL             "crl_file"
#define OPT_DEPTH           "verify_depth"
#define OPT_CIPHERS         "cipher_list"

#define OPT_FIFO            "fifo_size"
#ifdef HAVE_LINUX
#   define OPT_TXQUEUE         "txqueue"
#   define OPT_TUN_ONE_QUEUE   "tun_one_queue"
#endif
#define OPT_CLIENT_RATE     "client_max_rate"
#define OPT_CONNECTION_RATE "connection_max_rate"
#define OPT_TIMEOUT         "timeout"
#define OPT_KEEPALIVE       "keepalive"
#define OPT_MAX_CLIENTS     "max_clients"

#define OPT_DEFAULT_UP      "default_up"
#define OPT_DEFAULT_DOWN    "default_down"
#define OPT_UP              "up"
#define OPT_DOWN            "down"

#endif /*CONFIGURATION_H_*/

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
#include <openssl/err.h>

/* Option name and value lengths in the configuration value */
#define CONF_NAME_LENGTH 20
#define CONF_VALUE_LENGTH 200

struct configuration {
    int verbose;                                // verbose
    int debug;                                  // more verbose
    int daemonize;                              // daemonize the client
    struct in_addr localIP;                     // local IP address
    int localIP_set;                            // localIP is defined
    int localport;                              // local UDP port
    struct sockaddr_in serverAddr;              // rendezvous server inet address
    int serverIP_set;                           // serverAddr is defined
    struct in_addr vpnIP;                       // VPN IP address
    int vpnIP_set;                              // vpnIP is defined
    char network[CONF_VALUE_LENGTH];            // VPN subnetwork
    struct in_addr vpnBroadcastIP;              // "broadcast" IP, computed from vpnIP and network
    char iface[CONF_VALUE_LENGTH];              // bind to a specific network interface
    char certificate_pem[CONF_VALUE_LENGTH];    // PEM formated file containing the client certificate
    char key_pem[CONF_VALUE_LENGTH];            // PEM formated file containing the client private key
    char verif_pem[CONF_VALUE_LENGTH];          // PEM formated file containing the root certificates
    char cipher_list[CONF_VALUE_LENGTH];        // ciphers list for SSL_CTX_set_cipher_list
                                                // see openssl ciphers man page
    X509_CRL *crl;                              // The parsed CRL or NULL
    int FIFO_size;                              // Size of the FIFO list for the incoming packets
    int timeout;                                // wait timeout secs before closing a session for inactivity
};

extern void parseConfFile(char *file);

#endif /*CONFIGURATION_H_*/

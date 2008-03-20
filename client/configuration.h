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

// Longueur d'un champ et d'une valeur dans le fichier de conf
#define CONF_NAME_LENGTH 20
#define CONF_VALUE_LENGTH 200

struct configuration {
    int verbose;                                // verbeux
    int daemonize;                              // daemonize the client
    struct in_addr localIP;                     // adresse IP locale
    int localIP_set;                            // localIP est défini
    int localport;                              // port local
    struct sockaddr_in serverAddr;              // adresse du serveur de RDV
    int serverIP_set;                           // serverAddr est défini
    struct in_addr vpnIP;                       // adresse vpn
    int vpnIP_set;                              // vpnIP est défini
    char network[CONF_VALUE_LENGTH];            // réseau vpn
    struct in_addr vpnBroadcastIP;              // IP de "broadcast", calculée avec vpnIP et network
    char iface[CONF_VALUE_LENGTH];              // interface réseau spécifique
    char certificate_pem[CONF_VALUE_LENGTH];    // fichier certificat client au format PEM
    char key_pem[CONF_VALUE_LENGTH];            // fichier clé client au format PEM
    char verif_pem[CONF_VALUE_LENGTH];          // fichier contenant la chaine de certificats
                                                // pour la vérification au format PEM
    char cipher_list[CONF_VALUE_LENGTH];        // liste d'algos pour SSL_CTX_set_cipher_list
                                                // voir man d'openssl ciphers pour la syntaxe
    X509_CRL *crl;                              // la CRL décodée ou NULL si pas de CRL
    int FIFO_size;                              // Taille des FIFO de réception SSL, doit être > 10
    int timeout;                                // Délais d'inactivité (secondes) avant fermeture session
};

extern void parseConfFile(char *file);

#endif /*CONFIGURATION_H_*/

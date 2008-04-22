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
 
/*
 * Parse the configuration file
 * Check the configuration
 * Create the "config" structure containing all the configuration variables
 */
#include "campagnol.h"
#include "configuration.h"
#include "log.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <linux/if.h>
#include <sys/ioctl.h>

struct configuration config;

/*
 * Search the local IP address to use. Copy it into "ip" and set "localIPset".
 * If strlen(iface) != 0, get the IP associated with the given interface
 * Otherweise search the IP of the first interface different from lo or lo0
 * 
 * see http://groups.google.com/group/comp.os.linux.development.apps/msg/10f14dda86ee351a
 */
#define IFRSIZE   ((int)(size * sizeof (struct ifreq)))
void get_local_IP(struct in_addr * ip, int *localIPset, char *iface) {
    struct ifconf ifc;
    struct ifreq *ifr;
    int sockfd, size = 1;

    ifc.ifc_len = 0;
    ifc.ifc_req = NULL;

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    do {
        ++size;
        /* realloc buffer size until no overflow occurs  */
        if (NULL == (ifc.ifc_req = realloc(ifc.ifc_req, IFRSIZE))) {
            log_error("realloc");
            exit(EXIT_FAILURE);
        }
        ifc.ifc_len = IFRSIZE;
        if (ioctl(sockfd, SIOCGIFCONF, &ifc)) {
            log_error("ioctl SIOCGIFCONF");
            exit(EXIT_FAILURE);
        }
    } while  (IFRSIZE <= ifc.ifc_len);

    ifr = ifc.ifc_req;
    for (;(char *) ifr < (char *) ifc.ifc_req + ifc.ifc_len; ++ifr) {

//        if (ifr->ifr_addr.sa_data == (ifr+1)->ifr_addr.sa_data) {
//            continue; // duplicate, skip it
//        }
        
        if (strlen(iface) == 0 && (strcmp (ifr->ifr_name, "lo") == 0 || strcmp (ifr->ifr_name, "lo0") == 0)) {
            continue; // local interface, skip it
        }
        
        if (strlen(iface) == 0 || strcmp (ifr->ifr_name, iface) == 0) {
            *ip = (((struct sockaddr_in *) &(ifr->ifr_addr))->sin_addr);
            *localIPset = 1;
            break;
        }
    }
    close(sockfd);
    free(ifc.ifc_req);
}

/*
 * Get the broadcast IP for the VPN subnetwork
 * vpnip: IPv4 address of the client
 * len: number of bits in the netmask
 * broadcast (out): broadcast address
 * 
 * vpnIP and broadcast are in network byte order
 */
int get_ipv4_broadcast(uint32_t vpnip, int len, uint32_t *broadcast) {
    uint32_t netmask;
    if ( len<0 || len > 32 ) {// validity of len
        return -1;
    }
    // compute the netmask
    if (len == 32) {
        netmask = 0xffffffff;
    }
    else {
        netmask = ~(0xffffffff >> len);
    }
    // network byte order
    netmask = htonl(netmask);
    *broadcast = (vpnip & netmask) | ~netmask;
    return 0;
}


/*
 * Load the CRL file
 * Store the resulting X509_CRL structure in config.crl
 */
int load_CRL(char *crl_file) {
    X509_CRL *crl = NULL;
    BIO *bfile = NULL;
    bfile = BIO_new(BIO_s_file());
    
    // Lecture de la CRL à l'aide d'un BIO
    if (BIO_read_filename(bfile, crl_file) <= 0) {
        if (config.debug) fprintf(stderr, "load_CRL: BIO_read_filename\n");
        return -1;
    }
    crl = PEM_read_bio_X509_CRL(bfile, NULL, NULL, NULL);
    
    if (crl == NULL) {
        if (config.debug) fprintf(stderr, "load_CRL: fichier CRL non compris\n");
        return -1;
    }
    config.crl = crl;
    
    BIO_free(bfile);
    return 0;
}

/*
 * Main parsing function
 */
void parseConfFile(char *confFile) {
    FILE *conf = fopen(confFile, "r");
    if (conf == NULL) {
        log_error(confFile);
        exit(EXIT_FAILURE);
    }
    
    char line[1000];                    // last read line
    char *token;                        // word
    char name[CONF_NAME_LENGTH];        // option name
    char value[CONF_VALUE_LENGTH];      // option value
    
    int res;
    char *commentaire;
    
    /* set default values in config */
    memset(&config.localIP, 0, sizeof(config.localIP));
    config.localIP_set = 0;
    config.localport = 0;
    memset(&config.serverAddr, 0, sizeof(config.serverAddr));
    config.serverAddr.sin_family = AF_INET;
    config.serverAddr.sin_port=htons(SERVER_PORT_DEFAULT);
    config.serverIP_set = 0;
    memset(&config.vpnIP, 0, sizeof(config.localIP));
    config.vpnIP_set = 0;
    config.network[0] = '\0';
    config.iface[0] = '\0';
    config.certificate_pem[0] = '\0';
    config.key_pem[0] = '\0';
    config.verif_pem[0] = '\0';
    config.cipher_list[0] = '\0';
    config.crl = NULL;
    config.FIFO_size = 50;
    config.timeout = 120;
    config.max_clients = 100;
    
    /* Read the configuration file */
    while (fgets(line, sizeof(line), conf) != NULL) {
        // comment
        commentaire = strchr(line, '#');
        if (commentaire != NULL) {
            *commentaire = '\0';
        }
        
        // Read the name of the option
        token = strtok(line, " \t=\r\n");
        if (token != NULL) {
            strncpy(name, token, CONF_NAME_LENGTH);
        }
        else continue;
        
        // Read the value
        token = strtok(NULL, " \t=\r\n");
        if (token != NULL) {
            strncpy(value, token, CONF_VALUE_LENGTH);
        }
        else continue;
        
        
        if (strncmp(name, "local_host", CONF_NAME_LENGTH) == 0) {
            res = inet_aton(value, &config.localIP);
            if (res == 0) {
                struct hostent *host = gethostbyname(value);
                if (host==NULL) {
                    log_message("[%s:local_host] Local IP address or hostname is not valid: \"%s\"", confFile, value);
                    exit(EXIT_FAILURE);
                }
                memcpy(&(config.localIP.s_addr), host->h_addr_list[0], sizeof(struct in_addr));
            }
            config.localIP_set = 1;
        }
        else if (strncmp(name, "interface", CONF_NAME_LENGTH) == 0) {
            strncpy(config.iface, value, CONF_NAME_LENGTH);
        }
        else if (strncmp(name, "server_host", CONF_NAME_LENGTH) == 0) {
            res = inet_aton(value, &config.serverAddr.sin_addr);
            if (res == 0) {
                struct hostent *host = gethostbyname(value);
                if (host==NULL) {
                    log_message("[%s:server_host] RDV server IP address or hostname is not valid: \"%s\"", confFile, value);
                    exit(EXIT_FAILURE);
                }
                memcpy(&(config.serverAddr.sin_addr.s_addr), host->h_addr_list[0], sizeof(struct in_addr));
            }
            config.serverIP_set = 1;
        }
        else if (strncmp(name, "server_port", CONF_NAME_LENGTH) == 0) {
            /* get the server port */
            int port_tmp;
            if ( sscanf(value, "%ud", &port_tmp) != 1) {
                log_message("[%s:server_port] Server UDP port is not valid: \"%s\"", confFile, value);
                exit(EXIT_FAILURE);
            }
            config.serverAddr.sin_port = htons(port_tmp);
        }
        else if (strncmp(name, "local_port", CONF_NAME_LENGTH) == 0) {
            /* get the local port */
            if ( sscanf(value, "%d", &config.localport) != 1) {
                log_message("[%s:local_port] Local UDP port is not valid: \"%s\"", confFile, value);
                exit(EXIT_FAILURE);
            }
        }
        else if (strncmp(name, "vpn_ip", CONF_NAME_LENGTH) == 0) {
            /* Get the VPN IP address */
            if ( inet_aton(value, &config.vpnIP) == 0) {
                log_message("[%s:vpn_ip] VPN IP address is not valid: \"%s\"", confFile, value);
                exit(EXIT_FAILURE);
            }
            config.vpnIP_set = 1;
        }
        else if (strncmp(name, "network", CONF_NAME_LENGTH) == 0) {
            strncpy(config.network, value, CONF_VALUE_LENGTH);
        }
        else if (strncmp(name, "certificate", CONF_NAME_LENGTH) == 0) {
            strncpy(config.certificate_pem, value, CONF_VALUE_LENGTH);
        }
        else if (strncmp(name, "key", CONF_NAME_LENGTH) == 0) {
            strncpy(config.key_pem, value, CONF_VALUE_LENGTH);
        }
        else if (strncmp(name, "ca_certificates", CONF_NAME_LENGTH) == 0) {
            strncpy(config.verif_pem, value, CONF_VALUE_LENGTH);
        }
        else if (strncmp(name, "cipher_list", CONF_NAME_LENGTH) == 0) {
            strncpy(config.cipher_list, value, CONF_VALUE_LENGTH);
        }
        else if (strncmp(name, "crl_file", CONF_NAME_LENGTH) == 0) {
            if (load_CRL(value)) {
                log_message("[%s:crl_file] Error while loading the CRL file \"%s\"", confFile, value);
                //non fatal error
            }
        }
        else if (strncmp(name, "fifo_size", CONF_NAME_LENGTH) == 0) {
            if ( sscanf(value, "%d", &config.FIFO_size) != 1) {
                log_message("[%s:fifo_size] Internal FIFO size is not valid: \"%s\"", confFile, value);
                exit(EXIT_FAILURE);
            }
            if (config.FIFO_size <= 0) {
                log_message("[%s:fifo_size] Internal FIFO size %d must be > 0", confFile, config.FIFO_size);
                exit(EXIT_FAILURE);
            }
        }
        else if (strncmp(name, "timeout", CONF_NAME_LENGTH) == 0) {
            if ( sscanf(value, "%d", &config.timeout) != 1) {
                log_message("[%s:timeout] Timeout value is not valid: \"%s\"", confFile, value);
                exit(EXIT_FAILURE);
            }
            if (config.timeout < 5) {
                log_message("[%s:timeout] Timeout value %d must be >= 5", confFile, config.timeout);
                exit(EXIT_FAILURE);
            }
        }
        else if (strncmp(name, "max_clients", CONF_NAME_LENGTH) == 0) {
            if ( sscanf(value, "%d", &config.max_clients) != 1) {
                log_message("[%s:max_clients] Max number of clients is not valid: \"%s\"", confFile, value);
                exit(EXIT_FAILURE);
            }
            if (config.max_clients < 1) {
                log_message("[%s:max_clients] Max number of clients %d must be >= 1", confFile, config.timeout);
                exit(EXIT_FAILURE);
            }
        }
        
    }
    
    /* If no local IP address was given in the configuration file, 
     * try to get one with get_local_IP
     */
    if (config.localIP_set == 0)
       get_local_IP(&config.localIP, &config.localIP_set, config.iface);
    
    /* Still nothing :(
     * No connection, or wrong interface name
     */
    if (!config.localIP_set) {
        log_message("Could not find a valid local IP address. Please check %s", confFile);
        exit(EXIT_FAILURE);
    }
    
    
    /* Check the mandatory parameters */
    if (!config.serverIP_set) {
        log_message("[%s] Parameter \"server_host\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }
    if (!config.vpnIP_set) {
        log_message("[%s] Parameter \"vpn_ip\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }
    if (strlen(config.network) == 0) {
        log_message("[%s] Parameter \"network\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }
    if (strlen(config.certificate_pem) == 0) {
        log_message("[%s] Parameter \"certificate\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }
    if (strlen(config.key_pem) == 0) {
        log_message("[%s] Parameter \"key\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }
    if (strlen(config.verif_pem) == 0) {
        log_message("[%s] Parameter \"ca_certificates\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }
    
    /* compute the broadcast address */
    char * search, * end;
    int len;
    /* no netmask len? */
    if (!(search = strstr(config.network, "/"))) {
        log_message("[%s] Parameter \"network\" is not valid. Please give a netmask length (CIDR notation)", confFile);
        exit(EXIT_FAILURE);
    }
    else {
        search++;
        /* weird value */
        if (*search == '\0' || strlen(search) > 2) {
            log_message("[%s] Parameter \"network\": ill-formed netmask (1 or 2 figures)", confFile);
            exit(EXIT_FAILURE);
        }
        /* read the netmask */
        len = strtol(search, &end, 10);
        if ((end-search) != strlen(search)) {// A character is not a figure
            log_message("[%s] Parameter \"network\": ill-formed netmask (1 or 2 figures)", confFile);
            log_error("strtol:");
            exit(EXIT_FAILURE);
        }
    }
    /* get the broadcast IP */
    if (get_ipv4_broadcast(config.vpnIP.s_addr, len, &config.vpnBroadcastIP.s_addr)) {
        log_message("[%s] Parameter \"network\": ill-formed netmask, should be between 0 and 32", confFile);
        exit(EXIT_FAILURE);
    }

    fclose(conf);
}



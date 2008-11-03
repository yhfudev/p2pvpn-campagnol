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
 * Check the configuration
 * Create the "config" structure containing all the configuration variables
 */
#include "campagnol.h"
#include "configuration.h"
#include "config_parser.h"
#include "communication.h"
#include "log.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/ioctl.h>
#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif


struct configuration config;

#ifdef HAVE_IFADDRS_H
/*
 * Search the local IP address to use. Copy it into "ip" and set "localIPset".
 * If iface != NULL, get the IP associated with the given interface
 * Otherwise search the IP of the first non loopback interface
 */
void get_local_IP(struct in_addr * ip, int *localIPset, char *iface) {
    struct ifaddrs *ifap = NULL, *ifap_first = NULL;
    if (getifaddrs(&ifap) != 0) {
        log_error("getifaddrs");
        exit(EXIT_FAILURE);
    }

    ifap_first = ifap;
    while (ifap != NULL) {
        if (iface == NULL && (ifap->ifa_flags & IFF_LOOPBACK) != 0) {
            ifap = ifap->ifa_next;
            continue; // local interface, skip it
        }
        if (iface == NULL || strcmp (ifap->ifa_name, iface) == 0) {
            /* If the interface has no link level address (like a TUN device),
             * then ifap->ifa_addr is NULL.
             * Only look for AF_INET addresses
             */
            if (ifap->ifa_addr != NULL && ifap->ifa_addr->sa_family == AF_INET) {
                *ip = (((struct sockaddr_in *) ifap->ifa_addr)->sin_addr);
                *localIPset = 1;
                break;
            }
        }
        ifap = ifap->ifa_next;
    }
    freeifaddrs(ifap_first);
}
#else

/*
 * Search the local IP address to use. Copy it into "ip" and set "localIPset".
 * If iface != NULL, get the IP associated with the given interface
 * Otherwise search the IP of the first non loopback interface
 *
 * see http://groups.google.com/group/comp.os.linux.development.apps/msg/10f14dda86ee351a
 */
#define IFRSIZE   ((int)(size * sizeof (struct ifreq)))
void get_local_IP(struct in_addr * ip, int *localIPset, char *iface) {
    struct ifconf ifc;
    struct ifreq *ifr, ifreq_flags;
    int sockfd, size = 1;

    ifc.ifc_len = 0;
    ifc.ifc_req = NULL;

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    do {
        ++size;
        /* realloc buffer size until no overflow occurs  */
        ifc.ifc_req = CHECK_ALLOC_FATAL(realloc(ifc.ifc_req, IFRSIZE));
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

        strncpy(ifreq_flags.ifr_name,  ifr->ifr_name, IFNAMSIZ);
        if (ioctl(sockfd, SIOCGIFFLAGS, &ifreq_flags)) {
            log_error("ioctl SIOCGIFFLAGS");
            exit(EXIT_FAILURE);
        }
        if (ifreq_flags.ifr_flags & IFF_LOOPBACK) {
            continue; // local interface, skip it
        }

        if (iface == NULL || strcmp (ifr->ifr_name, iface) == 0) {
            *ip = (((struct sockaddr_in *) &(ifr->ifr_addr))->sin_addr);
            *localIPset = 1;
            break;
        }
    }
    close(sockfd);
    free(ifc.ifc_req);
}
#endif

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

    // Read the CRL with a BIO
    if (BIO_read_filename(bfile, crl_file) <= 0) {
        if (config.debug) fprintf(stderr, "load_CRL: BIO_read_filename\n");
        BIO_free(bfile);
        return -1;
    }
    crl = PEM_read_bio_X509_CRL(bfile, NULL, NULL, NULL);

    if (crl == NULL) {
        if (config.debug) fprintf(stderr, "load_CRL: CRL file is not valid\n");
        BIO_free(bfile);
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
    parser_context_t parser;
    char *value;
    int res;
    int nline;
    unsigned int port_tmp;

    int localIP_set = 0;    // config.localIP is defined

    /* set default values in config */
    memset(&config.localIP, 0, sizeof(config.localIP));
    config.localport = 0;
    memset(&config.serverAddr, 0, sizeof(config.serverAddr));
    config.serverAddr.sin_family = AF_INET;
    config.serverAddr.sin_port=htons(SERVER_PORT_DEFAULT);
    memset(&config.vpnIP, 0, sizeof(config.localIP));
    config.tun_mtu = TUN_MTU_DEFAULT;
    config.crl = NULL;
    config.FIFO_size = 50;
    config.timeout = 120;
    config.max_clients = 100;
    config.network = NULL;
    config.iface = NULL;
    config.certificate_pem = NULL;
    config.key_pem = NULL;
    config.verif_pem = NULL;
    config.cipher_list = NULL;
    config.pidfile = NULL;

    // init config parser. no DEFAULT section, no empty value
    parser_init(&parser, 0, 0, 1);

    // parse the file
    parser_read(confFile, &parser);

    /* now get, check and save each value */

    value = parser_get(SECTION_NETWORK, OPT_LOCAL_HOST, &nline, &parser);
    if (value != NULL) {
        res = inet_aton(value, &config.localIP);
        if (res == 0) {
            struct hostent *host = gethostbyname(value);
            if (host==NULL) {
                log_message("[%s:" OPT_LOCAL_HOST ":%d] Local IP address or hostname is not valid: \"%s\"", confFile, nline, value);
                exit(EXIT_FAILURE);
            }
            memcpy(&(config.localIP.s_addr), host->h_addr_list[0], sizeof(struct in_addr));
        }
        localIP_set = 1;
    }

    value = parser_get(SECTION_NETWORK, OPT_INTERFACE, &nline, &parser);
    if (value != NULL) {
        config.iface = CHECK_ALLOC_FATAL(strdup(value));
    }

    value = parser_get(SECTION_NETWORK, OPT_SERVER_HOST, &nline, &parser);
    if (value != NULL) {
        res = inet_aton(value, &config.serverAddr.sin_addr);
        if (res == 0) {
            struct hostent *host = gethostbyname(value);
            if (host==NULL) {
                log_message("[%s:"OPT_SERVER_HOST":%d] RDV server IP address or hostname is not valid: \"%s\"", confFile, nline, value);
                exit(EXIT_FAILURE);
            }
            memcpy(&(config.serverAddr.sin_addr.s_addr), host->h_addr_list[0], sizeof(struct in_addr));
        }
    }
    else {
        log_message("[%s] Parameter \""OPT_SERVER_HOST"\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }

    res = parser_getuint(SECTION_NETWORK, OPT_SERVER_PORT, &port_tmp, &value, &nline, &parser);
    if (res == 1) {
        config.serverAddr.sin_port = htons(port_tmp);
    }
    else if (res == 0) {
        log_message("[%s:"OPT_SERVER_PORT":%d] Server UDP port is not valid: \"%s\"", confFile, nline, value);
        exit(EXIT_FAILURE);
    }

    res = parser_getuint(SECTION_NETWORK, OPT_LOCAL_PORT, &config.localport, &value, &nline, &parser);
    if (res == 0) {
        log_message("[%s:"OPT_LOCAL_PORT":%d] Local UDP port is not valid: \"%s\"", confFile, nline, value);
        exit(EXIT_FAILURE);
    }

    res = parser_getint(SECTION_NETWORK, OPT_TUN_MTU, &config.tun_mtu, &value, &nline, &parser);
    if (res == 1) {
        if (config.tun_mtu < 150) {
            log_message("[%s:"OPT_TUN_MTU":%d] MTU of the tun device %d must be >= 150", confFile, nline, config.tun_mtu);
            exit(EXIT_FAILURE);
        }
    }
    else if (res == 0) {
        log_message("[%s:"OPT_TUN_MTU":%d] MTU of the tun device is not valid: \"%s\"", confFile, nline, value);
        exit(EXIT_FAILURE);
    }



    value = parser_get(SECTION_VPN, OPT_VPN_IP, &nline, &parser);
    if (value != NULL) {
        /* Get the VPN IP address */
        if ( inet_aton(value, &config.vpnIP) == 0) {
            log_message("[%s:"OPT_VPN_IP":%d] VPN IP address is not valid: \"%s\"", confFile, nline, value);
            exit(EXIT_FAILURE);
        }
    }
    else {
        log_message("[%s] Parameter \""OPT_VPN_IP"\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }

    value = parser_get(SECTION_VPN, OPT_VPN_NETWORK, &nline, &parser);
    if (value != NULL) {
        config.network = CHECK_ALLOC_FATAL(strdup(value));

        /* compute the broadcast address */
        char * search, * end;
        int len;
        /* no netmask len? */
        if (!(search = strchr(config.network, '/'))) {
            log_message("[%s] Parameter \""OPT_VPN_NETWORK"\" is not valid. Please give a netmask length (CIDR notation)", confFile);
            exit(EXIT_FAILURE);
        }
        else {
            search++;
            /* weird value */
            if (*search == '\0' || strlen(search) > 2) {
                log_message("[%s] Parameter \""OPT_VPN_NETWORK"\": ill-formed netmask (1 or 2 figures)", confFile);
                exit(EXIT_FAILURE);
            }
            /* read the netmask */
            len = strtol(search, &end, 10);
            if ((end-search) != strlen(search)) {// A character is not a figure
                log_message("[%s] Parameter \""OPT_VPN_NETWORK"\": ill-formed netmask (1 or 2 figures)", confFile);
                log_error("strtol:");
                exit(EXIT_FAILURE);
            }
        }
        /* get the broadcast IP */
        if (get_ipv4_broadcast(config.vpnIP.s_addr, len, &config.vpnBroadcastIP.s_addr)) {
            log_message("[%s] Parameter \""OPT_VPN_NETWORK"\": ill-formed netmask, should be between 0 and 32", confFile);
            exit(EXIT_FAILURE);
        }
    }
    else {
        log_message("[%s] Parameter \""OPT_VPN_NETWORK"\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }



    value = parser_get(SECTION_SECURITY, OPT_CERTIFICATE, &nline, &parser);
    if (value != NULL) {
        config.certificate_pem = CHECK_ALLOC_FATAL(strdup(value));
    }
    else {
        log_message("[%s] Parameter \""OPT_CERTIFICATE"\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }

    value = parser_get(SECTION_SECURITY, OPT_KEY, &nline, &parser);
    if (value != NULL) {
        config.key_pem = CHECK_ALLOC_FATAL(strdup(value));
    }
    else {
        log_message("[%s] Parameter \""OPT_KEY"\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }

    value = parser_get(SECTION_SECURITY, OPT_CA, &nline, &parser);
    if (value != NULL) {
        config.verif_pem = CHECK_ALLOC_FATAL(strdup(value));
    }
    else {
        log_message("[%s] Parameter \""OPT_CA"\" is mandatory", confFile);
        exit(EXIT_FAILURE);
    }

    value = parser_get(SECTION_SECURITY, OPT_CIPHERS, &nline, &parser);
    if (value != NULL) {
        config.cipher_list = CHECK_ALLOC_FATAL(strdup(value));
    }

    value = parser_get(SECTION_SECURITY, OPT_CRL, &nline, &parser);
    if (value != NULL) {
        if (load_CRL(value)) {
            log_message("[%s:"OPT_CRL":%d] Error while loading the CRL file \"%s\"", confFile, nline, value);
            //non fatal error
        }
    }



    res = parser_getint(SECTION_CLIENT, OPT_FIFO, &config.FIFO_size, &value, &nline, &parser);
    if (res == 1) {
        if (config.FIFO_size <= 0) {
            log_message("[%s:"OPT_FIFO":%d] Internal FIFO size %d must be > 0", confFile, nline, config.FIFO_size);
            exit(EXIT_FAILURE);
        }
    }
    else if (res == 0) {
        log_message("[%s:"OPT_FIFO":%d] Internal FIFO size is not valid: \"%s\"", confFile, nline, value);
        exit(EXIT_FAILURE);
    }

    res = parser_getint(SECTION_CLIENT, OPT_TIMEOUT, &config.timeout, &value, &nline, &parser);
    if (res == 1) {
        if (config.timeout < 5) {
            log_message("[%s:"OPT_TIMEOUT":%d] Timeout value %d must be >= 5", confFile, nline, config.timeout);
            exit(EXIT_FAILURE);
        }
    }
    else if (res == 0) {
        log_message("[%s:"OPT_TIMEOUT":%d] Timeout value is not valid: \"%s\"", confFile, nline, value);
        exit(EXIT_FAILURE);
    }

    res = parser_getint(SECTION_CLIENT, OPT_MAX_CLIENTS, &config.max_clients, &value, &nline, &parser);
    if (res == 1) {
        if (config.max_clients < 1) {
            log_message("[%s:"OPT_MAX_CLIENTS":%d] Max number of clients %d must be >= 1", confFile, nline, config.max_clients);
            exit(EXIT_FAILURE);
        }
    }
    else if (res == 0) {
        log_message("[%s:"OPT_MAX_CLIENTS":%d] Max number of clients is not valid: \"%s\"", confFile, nline, value);
        exit(EXIT_FAILURE);
    }

    value = parser_get(SECTION_CLIENT, OPT_PID_FILE, &nline, &parser);
    if (value != NULL) {
        config.pidfile = CHECK_ALLOC_FATAL(strdup(value));
    }



    /* If no local IP address was given in the configuration file,
     * try to get one with get_local_IP
     */
    if (localIP_set == 0)
       get_local_IP(&config.localIP, &localIP_set, config.iface);

    /* Still nothing :(
     * No connection, or wrong interface name
     */
    if (!localIP_set) {
        log_message("Could not find a valid local IP address. Please check %s", confFile);
        exit(EXIT_FAILURE);
    }


    parser_free(&parser);

}

void freeConfig() {
    if (config.crl != NULL) X509_CRL_free(config.crl);
    if (config.iface) free(config.iface);
    if (config.network) free(config.network);
    if (config.certificate_pem) free(config.certificate_pem);
    if (config.key_pem) free(config.key_pem);
    if (config.verif_pem) free(config.verif_pem);
    if (config.cipher_list) free(config.cipher_list);
    if (config.pidfile) free(config.pidfile);
}


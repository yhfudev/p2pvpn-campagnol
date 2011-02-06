/*
 * Campagnol configuration
 *
 * Copyright (C) 2008-2011 Florent Bondoux
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
#include "../common/config_parser.h"
#include "communication.h"
#include "../common/log.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/ioctl.h>
#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif
#ifdef HAVE_CYGWIN
#   include <w32api/windows.h>
#   include "tap-win32_common.h"
#endif

struct configuration config;

/* set default values in config */
void initConfig() {
    config.verbose = 0;
    config.daemonize = 0;
    config.debug = 0;

    memset(&config.localIP, 0, sizeof(config.localIP));
    config.localport = 0;
    memset(&config.serverAddr, 0, sizeof(config.serverAddr));
    config.serverAddr.sin_family = AF_INET;
    config.serverAddr.sin_port = htons(SERVER_PORT_DEFAULT);
    memset(&config.vpnIP, 0, sizeof(config.localIP));
    memset(&config.vpnNetmask, 0, sizeof(config.vpnNetmask));
    config.network = NULL;
    config.send_local_addr = 1;
    memset(&config.override_local_addr, 0, sizeof(config.override_local_addr));
    config.tun_mtu = TUN_MTU_DEFAULT;
    memset(&config.vpnBroadcastIP, 0, sizeof(config.vpnBroadcastIP));
    config.iface = NULL;
    config.tun_device = NULL;
    config.tap_id = NULL;

    config.certificate_pem = NULL;
    config.key_pem = NULL;
    config.verif_pem = NULL;
    config.verif_dir = NULL;
    config.verify_depth = 0;
    config.cipher_list = NULL;
    config.crl = NULL;

    config.FIFO_size = 20;
    config.tb_client_rate = 0.f;
    config.tb_connection_rate = 0.f;
    config.tb_client_size = 0;
    config.tb_connection_size = 0;
    config.timeout = 120;
    config.max_clients = 100;
    config.keepalive = 10;
    config.exec_up = NULL;
    config.exec_down = NULL;

    config.pidfile = NULL;

#ifdef HAVE_LINUX
    config.txqueue = 0;
    config.tun_one_queue = 0;
#endif
}

#ifdef HAVE_IFADDRS_H
/*
 * Search the local IP address to use. Copy it into "ip" and set "localIPset".
 * If iface != NULL, get the IP associated with the given interface
 * Otherwise search the IP of the first non loopback interface
 */
static int get_local_IP(struct in_addr * ip, int *localIPset, char *iface) {
    struct ifaddrs *ifap = NULL, *ifap_first = NULL;
    if (getifaddrs(&ifap) != 0) {
        log_error(errno, "getifaddrs");
        return -1;
    }

    ifap_first = ifap;
    while (ifap != NULL) {
        if (iface == NULL && ((ifap->ifa_flags & IFF_LOOPBACK)
                || !(ifap->ifa_flags & IFF_RUNNING)
                || !(ifap->ifa_flags & IFF_UP))) {
            ifap = ifap->ifa_next;
            continue; // local or not running interface, skip it
        }
        if (iface == NULL || strcmp(ifap->ifa_name, iface) == 0) {
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
    return 0;
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
static int get_local_IP(struct in_addr * ip, int *localIPset, char *iface) {
    struct ifconf ifc;
    struct ifreq *ifr, ifreq_flags;
    int sockfd, size = 1;
    struct in_addr ip_tmp;

    ifc.ifc_len = 0;
    ifc.ifc_req = NULL;

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    do {
        ++size;
        /* realloc buffer size until no overflow occurs  */
        ifc.ifc_req = CHECK_ALLOC_FATAL(realloc(ifc.ifc_req, IFRSIZE));
        ifc.ifc_len = IFRSIZE;
        if (ioctl(sockfd, SIOCGIFCONF, &ifc)) {
            log_error(errno, "ioctl SIOCGIFCONF");
            return -1;
        }
    }while (IFRSIZE <= ifc.ifc_len);

    ifr = ifc.ifc_req;
    for (;(char *) ifr < (char *) ifc.ifc_req + ifc.ifc_len; ++ifr) {

//        if (ifr->ifr_addr.sa_data == (ifr+1)->ifr_addr.sa_data) {
//            continue; // duplicate, skip it
//        }

        strncpy(ifreq_flags.ifr_name, ifr->ifr_name, IFNAMSIZ);
        if (ioctl(sockfd, SIOCGIFFLAGS, &ifreq_flags)) {
            log_error(errno, "ioctl SIOCGIFFLAGS");
            return -1;
        }
        if (iface == NULL && ((ifreq_flags.ifr_flags & IFF_LOOPBACK)
                || !(ifreq_flags.ifr_flags & IFF_RUNNING)
                || !(ifreq_flags.ifr_flags & IFF_UP))) {
            continue; // local or not running interface, skip it
        }

        if (iface == NULL || strcmp (ifr->ifr_name, iface) == 0) {
            ip_tmp = (((struct sockaddr_in *) &(ifr->ifr_addr))->sin_addr);
            if (ip_tmp.s_addr != INADDR_ANY && ip_tmp.s_addr != INADDR_NONE) {
                *ip = ip_tmp;
                *localIPset = 1;
                break;
            }
        }
    }
    close(sockfd);
    free(ifc.ifc_req);
    return 0;
}
#endif

/*
 * Get the broadcast IP for the VPN subnetwork
 * vpnip: IPv4 address of the client
 * len: number of bits in the netmask
 * broadcast (out): broadcast address
 * netmask (out): netmask
 *
 * vpnIP and broadcast are in network byte order
 */
static int get_ipv4_broadcast(uint32_t vpnip, int len, uint32_t *broadcast,
        uint32_t *netmask) {
    if (len < 0 || len > 32) {// validity of len
        return -1;
    }
    // compute the netmask
    if (len == 32) {
        *netmask = 0xffffffff;
    }
    else {
        *netmask = ~(0xffffffff >> len);
    }
    // network byte order
    *netmask = htonl(*netmask);
    *broadcast = (vpnip & *netmask) | ~*netmask;
    return 0;
}

int parseConfFile(const char *confFile) {
    parser_context_t parser;
    item_value_t *value;
    item_key_t *key;
    item_section_t *section;
    int ret = -1;
    int res;
    int default_commands, i, n;
    uint16_t port_tmp;

    int localIP_set = 0; // config.localIP is not defined

    // init config parser. no DEFAULT section, no empty value
    parser_init(&parser, 0, 0);

    // parse the file
    parser_read(confFile, &parser, config.debug);

    /* now get, check and save each value */
    value = parser_get(SECTION_NETWORK, OPT_LOCAL_HOST, -1, 1, &parser);
    if (value != NULL) {
        res = inet_aton(value->expanded.s, &config.localIP);
        if (res == 0) {
            struct hostent *host = gethostbyname(value->expanded.s);
            if (host == NULL) {
                log_message(
                        "[%s:" OPT_LOCAL_HOST ":%zu] Local IP address or hostname is not valid: \"%s\"",
                        confFile, value->nline, value->expanded.s);
                goto config_end;
            }
            memcpy(&(config.localIP.s_addr), host->h_addr_list[0],
                    sizeof(struct in_addr));
        }
        localIP_set = 1;
    }

    value = parser_get(SECTION_NETWORK, OPT_INTERFACE, -1, 1, &parser);
    if (value != NULL) {
#ifdef HAVE_CYGWIN
        // Try to replace the name in value by an interface GUID
        int found = 0;
        HKEY key_connections, key_iface;
        char iface_guid[256];
        char iface_key[1024];
        char adapter_name[16384];
        DWORD len;
        // List of network connections
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0,
                        KEY_READ, &key_connections) == ERROR_SUCCESS) {
            for (i = 0; !found; i++) {
                len = sizeof(iface_guid);
                // Enumerate over the connections
                if (RegEnumKeyEx(key_connections, i, iface_guid, &len, NULL,
                                NULL, NULL, NULL))
                break;

                snprintf(iface_key, sizeof(iface_key), "%s\\%s\\Connection",
                        NETWORK_CONNECTIONS_KEY, iface_guid);

                // Open the connection
                if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, iface_key, 0, KEY_READ,
                                &key_iface) == ERROR_SUCCESS) {
                    len = sizeof(adapter_name);
                    // Query its name
                    if (RegQueryValueEx(key_iface, "Name", NULL, NULL,
                                    (unsigned char *) adapter_name, &len)
                            == ERROR_SUCCESS) {
                        if (strcmp(value->expanded.s, adapter_name) == 0) {
                            config.iface
                            = CHECK_ALLOC_FATAL(strdup(iface_guid));
                            found = 1;
                            break;
                        }
                    }

                    RegCloseKey(key_iface);
                }
            }

            RegCloseKey(key_connections);
        }
        if (!found)
        config.iface = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
#else
        config.iface = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
#endif
    }

    value = parser_get(SECTION_NETWORK, OPT_SERVER_HOST, -1, 1, &parser);
    if (value != NULL) {
        res = inet_aton(value->expanded.s, &config.serverAddr.sin_addr);
        if (res == 0) {
            struct hostent *host = gethostbyname(value->expanded.s);
            if (host == NULL) {
                log_message(
                        "[%s:"OPT_SERVER_HOST":%zu] RDV server IP address or hostname is not valid: \"%s\"",
                        confFile, value->nline, value->expanded.s);
                goto config_end;
            }
            memcpy(&(config.serverAddr.sin_addr.s_addr), host->h_addr_list[0],
                    sizeof(struct in_addr));
        }
    }
    else {
        log_message("[%s] Parameter \""OPT_SERVER_HOST"\" is mandatory",
                confFile);
        goto config_end;
    }

    res = parser_get_ushort(SECTION_NETWORK, OPT_SERVER_PORT, -1, &port_tmp,
            &value, &parser);
    if (res == 1) {
        config.serverAddr.sin_port = htons(port_tmp);
    }
    else if (res == 0) {
        log_message(
                "[%s:"OPT_SERVER_PORT":%zu] Server UDP port is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    res = parser_get_ushort(SECTION_NETWORK, OPT_LOCAL_PORT, -1,
            &config.localport, &value, &parser);
    if (res == 0) {
        log_message(
                "[%s:"OPT_LOCAL_PORT":%zu] Local UDP port is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    res = parser_get_int(SECTION_NETWORK, OPT_TUN_MTU, -1, &config.tun_mtu,
            &value, &parser);
    if (res == 1) {
        if (config.tun_mtu < 150) {
            log_message(
                    "[%s:"OPT_TUN_MTU":%zu] MTU of the tun device %d must be >= 150",
                    confFile, value->nline, config.tun_mtu);
            goto config_end;
        }
    }
    else if (res == 0) {
        log_message(
                "[%s:"OPT_TUN_MTU":%zu] MTU of the tun device is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    res = parser_get_bool(SECTION_NETWORK, OPT_SEND_LOCAL, -1,
            &config.send_local_addr, &value, &parser);
    if (res == 0) {
        log_message(
                "[%s:"OPT_SEND_LOCAL":%zu] Invalid value (use \"yes\" or \"no\"): \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    if (config.send_local_addr == 1) {
        value = parser_get(SECTION_NETWORK, OPT_OVRRIDE_LOCAL, -1, 1, &parser);
        if (value != NULL) {
            char *address = value->expanded.s, *port, back;
            port = strchr(value->expanded.s, ' ');
            if (port != NULL) {
                back = *port;
                *port = '\0';
                res = inet_aton(address, &config.override_local_addr.sin_addr);
                if (res == 0) {
                    struct hostent *host = gethostbyname(address);
                    if (host == NULL) {
                        log_message(
                                "[%s:"OPT_OVRRIDE_LOCAL":%zu] IP address or hostname is not valid: \"%s\"",
                                confFile, value->nline, address);
                        goto config_end;
                    }
                    memcpy(&(config.override_local_addr.sin_addr.s_addr),
                            host->h_addr_list[0], sizeof(struct in_addr));
                }
                *port = back;
                res = sscanf(port, "%hu", &config.override_local_addr.sin_port);
                if (res != 1) {
                    log_message(
                            "[%s:"OPT_OVRRIDE_LOCAL":%zu] Port number is not valid: \"%s\"",
                            confFile, value->nline, port);
                    goto config_end;
                }
                config.override_local_addr.sin_port
                        = htons(config.override_local_addr.sin_port);
                config.send_local_addr = 2;
            }
            else {
                log_message(
                        "[%s:"OPT_OVRRIDE_LOCAL":%zu] Syntax error: \"%s\"",
                        confFile, value->nline, value->expanded.s);
                goto config_end;
            }
        }
    }

    value = parser_get(SECTION_NETWORK, OPT_TUN_DEVICE, -1, 1, &parser);
    if (value != NULL) {
        config.tun_device = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
    }

    value = parser_get(SECTION_NETWORK, OPT_TAP_ID, -1, 1, &parser);
    if (value != NULL) {
        config.tap_id = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
    }

    value = parser_get(SECTION_VPN, OPT_VPN_IP, -1, 1, &parser);
    if (value != NULL) {
        /* Get the VPN IP address */
        if (inet_aton(value->expanded.s, &config.vpnIP) == 0) {
            log_message(
                    "[%s:"OPT_VPN_IP":%zu] VPN IP address is not valid: \"%s\"",
                    confFile, value->nline, value->expanded.s);
            goto config_end;
        }
    }
    else {
        log_message("[%s] Parameter \""OPT_VPN_IP"\" is mandatory", confFile);
        goto config_end;
    }

    value = parser_get(SECTION_VPN, OPT_VPN_NETWORK, -1, 1, &parser);
    if (value != NULL) {
        config.network = CHECK_ALLOC_FATAL(strdup(value->expanded.s));

        /* compute the broadcast address */
        char * search, *end;
        int len;
        /* no netmask len? */
        if (!(search = strchr(config.network, '/'))) {
            log_message(
                    "[%s] Parameter \""OPT_VPN_NETWORK"\" is not valid. Please give a netmask length (CIDR notation)",
                    confFile);
            goto config_end;
        }
        else {
            search++;
            /* weird value */
            if (*search == '\0' || strlen(search) > 2) {
                log_message(
                        "[%s] Parameter \""OPT_VPN_NETWORK"\": ill-formed netmask (1 or 2 figures)",
                        confFile);
                goto config_end;
            }
            /* read the netmask */
            len = (int) strtol(search, &end, 10);
            if ((unsigned int) (end - search) != strlen(search)) {// A character is not a figure
                log_message(
                        "[%s] Parameter \""OPT_VPN_NETWORK"\": ill-formed netmask (1 or 2 figures)",
                        confFile);
                goto config_end;
            }
        }
        /* get the broadcast IP */
        if (get_ipv4_broadcast(config.vpnIP.s_addr, len,
                &config.vpnBroadcastIP.s_addr, &config.vpnNetmask.s_addr)) {
            log_message(
                    "[%s] Parameter \""OPT_VPN_NETWORK"\": ill-formed netmask, should be between 0 and 32",
                    confFile);
            goto config_end;
        }
    }
    else {
        log_message("[%s] Parameter \""OPT_VPN_NETWORK"\" is mandatory",
                confFile);
        goto config_end;
    }

    value = parser_get(SECTION_SECURITY, OPT_CERTIFICATE, -1, 1, &parser);
    if (value != NULL) {
        config.certificate_pem = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
    }
    else {
        log_message("[%s] Parameter \""OPT_CERTIFICATE"\" is mandatory",
                confFile);
        goto config_end;
    }

    value = parser_get(SECTION_SECURITY, OPT_KEY, -1, 1, &parser);
    if (value != NULL) {
        config.key_pem = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
    }
    else {
        log_message("[%s] Parameter \""OPT_KEY"\" is mandatory", confFile);
        goto config_end;
    }

    value = parser_get(SECTION_SECURITY, OPT_CA, -1, 1, &parser);
    if (value != NULL) {
        config.verif_pem = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
    }
    value = parser_get(SECTION_SECURITY, OPT_CA_DIR, -1, 1, &parser);
    if (value != NULL) {
        config.verif_dir = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
    }
    if (config.verif_pem == NULL && config.verif_dir == NULL) {
        log_message(
                "[%s] At least one of \""OPT_CA"\" and \""OPT_CA_DIR"\"is required",
                confFile);
        goto config_end;
    }

    res = parser_get_int(SECTION_SECURITY, OPT_DEPTH, -1, &config.verify_depth,
            &value, &parser);
    if (res == 1) {
        if (config.verify_depth <= 0) {
            log_message(
                    "[%s:"OPT_DEPTH":%zu] Maximum depth for certificate verification %d must be > 0",
                    confFile, value->nline, config.tun_mtu);
            goto config_end;
        }
    }
    else if (res == 0) {
        log_message(
                "[%s:"OPT_DEPTH":%zu] Maximum depth for certificate verification is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    value = parser_get(SECTION_SECURITY, OPT_CIPHERS, -1, 1, &parser);
    if (value != NULL) {
        config.cipher_list = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
    }

    value = parser_get(SECTION_SECURITY, OPT_CRL, -1, 1, &parser);
    if (value != NULL) {
        config.crl = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
    }

    res = parser_get_int(SECTION_CLIENT, OPT_FIFO, -1, &config.FIFO_size,
            &value, &parser);
    if (res == 1) {
        if (config.FIFO_size <= 0) {
            log_message(
                    "[%s:"OPT_FIFO":%zu] Internal FIFO size %d must be > 0",
                    confFile, value->nline, config.FIFO_size);
            goto config_end;
        }
    }
    else if (res == 0) {
        log_message(
                "[%s:"OPT_FIFO":%zu] Internal FIFO size is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

#ifdef HAVE_LINUX
    res = parser_get_int(SECTION_CLIENT, OPT_TXQUEUE, -1, &config.txqueue,
            &value, &parser);
    if (res == 1) {
        if (config.txqueue <= 0) {
            log_message(
                    "[%s:"OPT_TXQUEUE":%zu] TUN/TAP transmit queue length %d must be > 0",
                    confFile, value->nline, config.txqueue);
            goto config_end;
        }
    }
    else if (res == 0) {
        log_message(
                "[%s:"OPT_TXQUEUE":%zu] TUN/TAP transmit queue length is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    res = parser_get_bool(SECTION_CLIENT, OPT_TUN_ONE_QUEUE, -1,
            &config.tun_one_queue, &value, &parser);
    if (res == 0) {
        log_message(
                "[%s:"OPT_TUN_ONE_QUEUE":%zu] Invalid value (use \"yes\" or \"no\"): \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }
#endif

    res = parser_get_float(SECTION_CLIENT, OPT_CLIENT_RATE, -1,
            &config.tb_client_rate, &value, &parser);
    if (res == 1) {
        if (config.tb_client_rate < 0) {
            log_message(
                    "[%s:"OPT_CLIENT_RATE":%zu] Rate limite %f must be > 0",
                    confFile, value->nline, config.tb_client_rate);
            goto config_end;
        }
    }
    else if (res == 0 && strlen(value->expanded.s) > 0) {
        log_message(
                "[%s:"OPT_CLIENT_RATE":%zu] Rate limite is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    res = parser_get_float(SECTION_CLIENT, OPT_CONNECTION_RATE, -1,
            &config.tb_connection_rate, &value, &parser);
    if (res == 1) {
        if (config.tb_connection_rate < 0) {
            log_message(
                    "[%s:"OPT_CONNECTION_RATE":%zu] Rate limite %f must be > 0",
                    confFile, value->nline, config.tb_connection_rate);
            goto config_end;
        }
    }
    else if (res == 0 && strlen(value->expanded.s) > 0) {
        log_message(
                "[%s:"OPT_CONNECTION_RATE":%zu] Rate limite is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    res = parser_get_int(SECTION_CLIENT, OPT_TIMEOUT, -1, &config.timeout,
            &value, &parser);
    if (res == 1) {
        if (config.timeout < 5) {
            log_message("[%s:"OPT_TIMEOUT":%zu] Timeout value %d must be >= 5",
                    confFile, value->nline, config.timeout);
            goto config_end;
        }
    }
    else if (res == 0) {
        log_message(
                "[%s:"OPT_TIMEOUT":%zu] Timeout value is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    res = parser_get_int(SECTION_CLIENT, OPT_MAX_CLIENTS, -1,
            &config.max_clients, &value, &parser);
    if (res == 1) {
        if (config.max_clients < 1) {
            log_message(
                    "[%s:"OPT_MAX_CLIENTS":%zu] Max number of clients %d must be >= 1",
                    confFile, value->nline, config.max_clients);
            goto config_end;
        }
    }
    else if (res == 0) {
        log_message(
                "[%s:"OPT_MAX_CLIENTS":%zu] Max number of clients is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    res = parser_get_uint(SECTION_CLIENT, OPT_KEEPALIVE, -1, &config.keepalive,
            &value, &parser);
    if (res == 1) {
        if (config.keepalive < 1) {
            log_message(
                    "[%s:"OPT_KEEPALIVE":%zu] Keepalive interval %u must be >=1",
                    confFile, value->nline, config.keepalive);
            goto config_end;
        }
    }
    else if (res == 0) {
        log_message(
                "[%s:"OPT_KEEPALIVE":%zu] Keepalive interval is not valid: \"%s\"",
                confFile, value->nline, value->expanded.s);
        goto config_end;
    }

    /* If no local IP address was given in the configuration file,
     * try to get one with get_local_IP
     */
    if (localIP_set == 0) {
        if (get_local_IP(&config.localIP, &localIP_set, config.iface) != 0)
            goto config_end;
    }

    /* Still nothing :(
     * No connection, or wrong interface name
     */
    if (!localIP_set) {
        log_message("Could not find a valid local IP address. Please check %s",
                confFile);
        goto config_end;
    }

    /* Need a non-"any" local IP if config.send_local_addr is true */
    if (config.send_local_addr == 1 && config.localIP.s_addr == INADDR_ANY) {
        log_message(
                "["SECTION_NETWORK"]" OPT_SEND_LOCAL" requires a valid local IP (option \""OPT_LOCAL_HOST"\")");
        goto config_end;
    }

    /* Define the bucket size for the rate limiters:
     * Max(3*MESSAGE_MAX_LENGTH, 0.5s*rate)
     */
    if (config.tb_client_rate > 0) {
        config.tb_client_size = (size_t) (500 * config.tb_client_rate);
        if (config.tb_client_size < (3 * MESSAGE_MAX_LENGTH))
            config.tb_client_size = (size_t) 3 * MESSAGE_MAX_LENGTH;
    }
    if (config.tb_connection_rate > 0) {
        config.tb_connection_size = (size_t) (500 * config.tb_connection_rate);
        if (config.tb_connection_size < (3 * MESSAGE_MAX_LENGTH))
            config.tb_connection_size = (size_t) 3 * MESSAGE_MAX_LENGTH;
    }

    /* get the shell commands in COMMANDS
     * copy them into config.exec_up and config.exec_down
     */
    section = parser_section_get(SECTION_COMMANDS, &parser);
    if (section) {
        res = parser_get_bool(SECTION_COMMANDS, OPT_DEFAULT_UP, -1,
                &default_commands, &value, &parser);
        if (res == 1 && !default_commands) {
            key = parser_key_get(OPT_UP, section);
            if (key == NULL)
                n = 0;
            else
                n = parser_key_get_nvalues(key);
            config.exec_up = CHECK_ALLOC_FATAL(malloc(sizeof(char *) * (n+1)));
            config.exec_up[n] = NULL;
            if (n != 0) {
                i = 0;
                TAILQ_FOREACH(value, &key->values_list, tailq) {
                    parser_value_expand(section, value);
                    config.exec_up[i] = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
                    i++;
                }
            }
        }

        res = parser_get_bool(SECTION_COMMANDS, OPT_DEFAULT_DOWN, -1,
                &default_commands, &value, &parser);
        if (res == 1 && !default_commands) {
            key = parser_key_get(OPT_DOWN, section);
            if (key == NULL)
                n = 0;
            else
                n = parser_key_get_nvalues(key);
            config.exec_down
                    = CHECK_ALLOC_FATAL(malloc(sizeof(char *) * (n+1)));
            config.exec_down[n] = NULL;
            if (n != 0) {
                i = 0;
                TAILQ_FOREACH(value, &key->values_list, tailq) {
                    parser_value_expand(section, value);
                    config.exec_down[i] = CHECK_ALLOC_FATAL(strdup(value->expanded.s));
                    i++;
                }
            }
        }
    }

    ret = 0;

    config_end:

    parser_free(&parser);
    return ret;
}

void freeConfig() {
    char **s;
    if (config.crl) free(config.crl);
    if (config.iface) free(config.iface);
    if (config.network) free(config.network);
    if (config.certificate_pem) free(config.certificate_pem);
    if (config.key_pem) free(config.key_pem);
    if (config.verif_pem) free(config.verif_pem);
    if (config.verif_dir) free(config.verif_dir);
    if (config.cipher_list) free(config.cipher_list);
    if (config.pidfile) free(config.pidfile);
    if (config.tun_device) free(config.tun_device);
    if (config.tap_id) free(config.tap_id);
    if (config.exec_up) {
        s = config.exec_up;
        while (*s) {
            free(*s);
            s++;
        }
        free(config.exec_up);
    }
    if (config.exec_down) {
        s = config.exec_down;
        while (*s) {
            free(*s);
            s++;
        }
        free(config.exec_down);
    }
}


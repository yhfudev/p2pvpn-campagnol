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
 * Lecture et analyse du fichier de configuration
 * Vérification de la configuration
 * Création de la structure "config" contenant toutes les 
 * variables de configuration
 */

#include "campagnol.h"
#include "configuration.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <linux/if.h>
#include <sys/ioctl.h>

struct configuration config;

/* Obtenir l'adresse IP associée soit à l'interface
 * donnée dans iface, soit à la première interface trouvée
 * différente de lo si strlen(iface) == 0
 * positionne localIPSet si IP trouvée
 * 
 * voir http://groups.google.com/group/comp.os.linux.development.apps/msg/10f14dda86ee351a
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
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        ifc.ifc_len = IFRSIZE;
        if (ioctl(sockfd, SIOCGIFCONF, &ifc)) {
            perror("ioctl SIOCFIFCONF");
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

/* Obtenir l'IP "broadcast" pour le réseau VPN
 * vpnip : ipv4 VPN du client
 * len : nombre de bits du netmask
 * broadcast (out) : adresse broadcast
 * vpnip et broadcast sont en network byte order
 */
int get_ipv4_broadcast(uint32_t vpnip, int len, uint32_t *broadcast) {
    uint32_t netmask;
    if ( len<0 || len > 32 ) {// nombre de bits du netmask non valide
        return -1;
    }
    // calcul du netmask
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


/* Charger la liste de révocation de certificats si
 * elle est donnée
 * Résultat placé dans config.crl
 */
int load_CRL(char *crl_file) {
    X509_CRL *crl = NULL;
    BIO *bfile = NULL;
    bfile = BIO_new(BIO_s_file());
    
    // Lecture de la CRL à l'aide d'un BIO
    if (BIO_read_filename(bfile, crl_file) <= 0) {
        if (config.verbose) fprintf(stderr, "load_CRL: BIO_read_filename\n");
        return -1;
    }
    crl = PEM_read_bio_X509_CRL(bfile, NULL, NULL, NULL);
    
    if (crl == NULL) {
        if (config.verbose) fprintf(stderr, "load_CRL: fichier CRL non compris\n");
        return -1;
    }
    config.crl = crl;
    
    BIO_free(bfile);
    return 0;
}


void parseConfFile(char *confFile) {
    FILE *conf = fopen(confFile, "r");
    if (conf == NULL) {
        perror(confFile);
        exit(1);
    }
    
    char line[1000];                    // ligne lue
    char *token;                        // mot lu
    char name[CONF_NAME_LENGTH];        // nom d'option
    char value[CONF_VALUE_LENGTH];      // valeur d'option
    
    int res;
    char *commentaire;
    
    // Initialisation de config
    memset(&config.localIP, 0, sizeof(config.localIP));
    config.localIP_set = 0;
    config.localport = 0;
    memset(&config.serverAddr, 0, sizeof(config.serverAddr));
    config.serverAddr.sin_family = AF_INET;
    config.serverAddr.sin_port=htons(SERVER_PORT);
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
    
    // Lecture ligne par ligne du fichier de conf
    while (fgets(line, sizeof(line), conf) != NULL) {
        // Commentaire sur la ligne
        commentaire = strchr(line, '#');
        if (commentaire != NULL) {
            *commentaire = '\0';
        }
        
        // Lecture du nom de l'option
        token = strtok(line, " \t=\r\n");
        if (token != NULL) {
            strncpy(name, token, CONF_NAME_LENGTH);
        }
        else continue;
        
        // Lecture de la valeur
        token = strtok(NULL, " \t=\r\n");
        if (token != NULL) {
            strncpy(value, token, CONF_VALUE_LENGTH);
        }
        else continue;
        
        
        // Cas par cas
        if (strncmp(name, "local_host", CONF_NAME_LENGTH) == 0) {
            res = inet_aton(value, &config.localIP);
            if (res == 0) {
                struct hostent *host = gethostbyname(value);
                if (host==NULL) {
                    fprintf(stderr, "[%s:local_host] Adresse IP locale non valide\n", confFile);
                    exit(1);
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
                    fprintf(stderr, "[%s:server_host] Adresse IP serveur non comprise \"%s\"\n", confFile, value);
                    exit(1);
                }
                memcpy(&(config.serverAddr.sin_addr.s_addr), host->h_addr_list[0], sizeof(struct in_addr));
            }
            config.serverIP_set = 1;
        }
        else if (strncmp(name, "local_port", CONF_NAME_LENGTH) == 0) {
            /** Recuperation du port local */
            if ( sscanf(value, "%d", &config.localport) != 1) {
                fprintf(stderr, "[%s:local_port] Port local non valide\n", confFile);
                exit(1);
            }
        }
        else if (strncmp(name, "vpn_ip", CONF_NAME_LENGTH) == 0) {
            /** Recuperation de l'adresse VPN */
            if ( inet_aton(value, &config.vpnIP) == 0) {
                fprintf(stderr, "[%s:vpn_ip] Adresse IP VPN non comprise\n", confFile);
                exit(1);
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
                fprintf(stderr, "[%s:crl_file] Erreur lors du chargement du fichier\n", confFile);
                //erreur non fatale
            }
        }
        else if (strncmp(name, "fifo_size", CONF_NAME_LENGTH) == 0) {
            if ( sscanf(value, "%d", &config.FIFO_size) != 1) {
                fprintf(stderr, "[%s:fifo_size] Taille FIFO non valide\n", confFile);
                exit(1);
            }
            if (config.FIFO_size <= 0) {
                fprintf(stderr, "[%s:fifo_size] La taille de la FIFO doit être > 0\n", confFile);
                exit(1);
            }
        }
        else if (strncmp(name, "timeout", CONF_NAME_LENGTH) == 0) {
            if ( sscanf(value, "%d", &config.timeout) != 1) {
                fprintf(stderr, "[%s:timeout] Timeout non valide\n", confFile);
                exit(1);
            }
            if (config.timeout < 5) {
                fprintf(stderr, "[%s:timeout] Timeout doit être >= 5\n", confFile);
                exit(1);
            }
        }
        
    }
    
    // Si aucune adresse IP locale n'a été donnée, on tente de l'obtenir avec get_local_IP
    if (config.localIP_set == 0) // pas d'IP locale passée en argument
       get_local_IP(&config.localIP, &config.localIP_set, config.iface);
    
    // Si toujours rien
    if (!config.localIP_set) { // non connecté, ou nom d'interface incorrect
        fprintf(stderr, "Aucune adresse IP locale trouvée\n");
        exit(1);
    }
    
    
    // Vérification des paramètres obligatoires
    if (!config.serverIP_set) {
        fprintf(stderr, "[%s] Paramètre \"server_host\" obligatoire\n", confFile);
        exit(1);
    }
    if (!config.vpnIP_set) {
        fprintf(stderr, "[%s] Paramètre \"vpn_ip\" obligatoire\n", confFile);
        exit(1);
    }
    if (strlen(config.network) == 0) {
        fprintf(stderr, "[%s] Paramètre \"network\" obligatoire\n", confFile);
        exit(1);
    }
    if (strlen(config.certificate_pem) == 0) {
        fprintf(stderr, "[%s] Paramètre \"certificate\" obligatoire\n", confFile);
        exit(1);
    }
    if (strlen(config.key_pem) == 0) {
        fprintf(stderr, "[%s] Paramètre \"key\" obligatoire\n", confFile);
        exit(1);
    }
    if (strlen(config.verif_pem) == 0) {
        fprintf(stderr, "[%s] Paramètre \"ca_certificates\" obligatoire\n", confFile);
        exit(1);
    }
    
    // Calculer l'adresse de broadcast
    char * search, * end;
    int len;
    // pas de netmask : 32 bits par défaut
    if (!(search = strstr(config.network, "/"))) {
        len = 32;
    }
    else {
        search++;
        // valeur nulle ou de taille bizarre
        if (*search == '\0' || strlen(search) > 2) {
            fprintf(stderr, "[%s] Paramère \"network\" : netmask mal formé (1 ou 2 chiffres)\n", confFile);
            exit(1);
        }
        // lire le netmask
        len = strtol(search, &end, 10);
        if ((end-search) != strlen(search)) {// un caractère n'est pas un nombre
            fprintf(stderr, "[%s] Paramère \"network\" : netmask mal formé (1 ou 2 chiffres)\n", confFile);
            perror("strtol:");
            exit(1);
        }
    }
    // obtenir ip broadcast
    if (get_ipv4_broadcast(config.vpnIP.s_addr, len, &config.vpnBroadcastIP.s_addr)) {
        fprintf(stderr, "[%s] Paramère \"network\" : netmask mal formé (entre 0 et 32)\n", confFile);
        exit(1);
    }

    fclose(conf);
}



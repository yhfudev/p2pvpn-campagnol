/*
 * Startup code
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

#include "campagnol.h"

#include <signal.h>

#include <sys/ioctl.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h> 
#include <getopt.h>


#include "tun_device.h"
#include "net_socket.h"
#include "communication.h"
#include "configuration.h"
#include "log.h"
#include "pthread_wrap.h"

int end_campagnol = 0;


void usage(void) {
    fprintf(stderr, "Usage: campagnol [OPTION]... [configuration_file]\n\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, " -v, --verbose\t\t\tverbose mode\n");
    fprintf(stderr, " -D, --daemon\t\t\tfork in background\n");
    fprintf(stderr, " -d, --debug\t\t\tdebug mode\n");
    fprintf(stderr, " -h, --help\t\t\tthis help message\n");
    fprintf(stderr, " -V, --version\t\t\tshow version information and exit\n\n");
    fprintf(stderr, "If no configuration file is given, the default is /etc/campagnol.conf\n");
    exit(EXIT_FAILURE);
}

void version(void) {
    fprintf(stderr, "Campagnol VPN | Client | Version %s\n", VERSION);
    fprintf(stderr, "Copyright (c) 2007 Antoine Vianey\n");
    fprintf(stderr, "              2008 Florent Bondoux\n");
    exit(EXIT_SUCCESS);
}

int parse_args(int argc, char **argv, char **configFile) {
    int opt;
    
    config.verbose = 0;
    config.daemonize = 0;
    config.debug = 0;
    struct option long_options[] = {
        {"verbose", 0, NULL, 'v'},
        {"daemon", 0, NULL, 'D'},
        {"debug", 0, NULL, 'd'},
        {"version", 0, NULL, 'V'},
        {"help", 0, NULL, 'h'},
        {0, 0, 0, 0}
    };
    while ((opt = getopt_long(argc, argv, "vVdDh", long_options, NULL)) >= 0) {
        switch(opt) {
            case 'v':
                config.verbose = 1;
                break;
            case 'D' :
                config.daemonize = 1;
                break;
            case 'd' :
                config.debug = 1;
                config.verbose = 1;
                break;
            case 'V' :
                return 2;
            case 'h' :
                return 1;
            default : return 1;
        }
    }
    
    argv += optind;
    argc -= optind;
    
    /* the configuration file */
    if (argc == 1) {
        *configFile = argv[0];
    }
    else if (argc == 0) {
        *configFile = DEFAULT_CONF_FILE;
    }
    else {
        return 1;
    }
    
    return 0;
}

void daemonize(void) {
    int r;

    printf("Going in background...\n");
    r = daemon(1, 0);
    if (r != 0) {
        log_error("Unable to daemonize");
        exit(EXIT_FAILURE);
    }

    config.verbose = 0;
    config.debug = 0;
}

/* thread used to cath and handle signals */
void * sig_handler(void * arg) {
    sigset_t mask;
    int sig;
    
    while (1) {
        sigfillset(&mask);
        sigwait(&mask, &sig); 
        
        switch (sig) {
            case SIGTERM:
            case SIGINT:
                end_campagnol = 1;
                log_message("received signal %d, exiting...", sig);
                return NULL;
            case SIGALRM:
                handler_sigTimerPing(sig);
                break;
            default:
                break;
        }
    }
    
}


int main (int argc, char **argv) {
    char *configFile = NULL;
    int sockfd, tunfd;
    int pa;
    
    pa = parse_args(argc, argv, &configFile);
    if (pa == 1) {
        usage();
    }
    else if (pa == 2) {
        version();
    }
    
    if (config.daemonize) daemonize();
    log_init(config.daemonize, "campagnol");
    
    parseConfFile(configFile);
    /* Print the current OpenSSL error stack (missing CRL file)
     * Empty the error stack
     */
    ERR_print_errors_fp(stderr);
    
    if (config.verbose) {
        puts("Configuration:");
        printf("  Local IP address: %s\n", inet_ntoa (config.localIP));
        if (strlen(config.iface) != 0) printf("  Using interface: %s\n", config.iface);
        if (config.localport != 0) printf("  Using local port: %d\n", config.localport);
        printf("  RDV server IP address: %s\n", inet_ntoa (config.serverAddr.sin_addr));
        printf("  RDV server port: %d\n", ntohs(config.serverAddr.sin_port));
        printf("  VPN IP addres: %s\n", inet_ntoa(config.vpnIP));
        printf("  VPN broadcast IP: %s\n", inet_ntoa(config.vpnBroadcastIP));
        printf("  VPN subnetwork: %s\n", config.network);
        printf("  DTLS certificate file: %s\n", config.certificate_pem);
        printf("  DTLS private key file: %s\n", config.key_pem);
        printf("  DTLS root certificates chain file: %s\n", config.verif_pem);
        if (strlen(config.cipher_list) != 0) printf("  DTLS cipher list: %s\n", config.cipher_list);
        if (config.crl) printf("  Using a certificate revocation list (%d entries)\n", sk_num(X509_CRL_get_REVOKED(config.crl)));
        printf("  FIFO size: %d\n", config.FIFO_size);
        printf("  Timeout: %d sec.\n", config.timeout);
        printf("  Maximum number of connections: %d\n\n", config.max_clients);
    }
    
    sockfd = create_socket();
    if (sockfd < 0) {
        exit(EXIT_FAILURE);
    }
    
    
    /* mask all signals in this thread and child threads */
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    
    /* start the signal handler */
    createThread(sig_handler, NULL);
    
    
    /* The UDP socket is configured
     * Now, register to the rendezvous server
     */
    if (register_rdv(sockfd)) {
        exit(EXIT_FAILURE);
    }

    tunfd = init_tun(1);
    if (tunfd < 0) {
        struct message smsg;
        smsg.ip1.s_addr = config.vpnIP.s_addr;
        smsg.ip2.s_addr = 0;
        smsg.port = 0;
        smsg.type = BYE;
        if (config.debug) printf("Sending BYE\n");
        sendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
        exit(EXIT_FAILURE);
    }
    
    log_message("Starting VPN");
    
    start_vpn(sockfd, tunfd);
    
    log_close();
    close_tun(tunfd);
    close(sockfd);
    exit(EXIT_SUCCESS);
}

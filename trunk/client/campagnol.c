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

#include <signal.h>

#include <sys/ioctl.h>

#include <arpa/inet.h>
#include <linux/if.h>
#include <netdb.h>
#include <sys/stat.h> 
#include <getopt.h>


#include "campagnol.h"
#include "tun_device.h"
#include "net_socket.h"
#include "communication.h"
#include "configuration.h"

int end_campagnol = 0;

void handler_term(int s) {
    end_campagnol = 1;
    printf("received signal %d, exiting...\n", s);
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
}


void usage(void) {
    fprintf(stderr, "Usage: campagnol [OPTION]... configuration_file\n\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, " -v, --verbose\t\t\tverbose mode\n");
    fprintf(stderr, " -D, --daemon\t\t\tfork in background\n");
    fprintf(stderr, " -d, --debug\t\t\tdebug mode\n");
    fprintf(stderr, " -h, --help\t\t\tthis help message\n");
    exit(1);
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
        {"help", 0, NULL, 'h'}
    };
    while ((opt = getopt_long(argc, argv, "vdDh", long_options, NULL)) >= 0) {
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
            case 'h' :
                return 1;
            default : return 1;
        }
    }
    
    argv += optind;
    argc -= optind;
    
    if (argc != 1) {
        return 1;
    }
    
    /* the configuration file */
    *configFile = argv[0];
    
    return 0;
}

void daemonize(void) {
    pid_t pid;
    
    if (getppid() == 1) { // parent process
        return;        
    }
    
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid > 0) {
        printf("Going in background with pid %d\n", pid);
        exit(0);
    }
    
    umask(0);
    
    if (setsid() < 0) {
        perror("setsid");
        exit(1);
    }
    
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);
    config.verbose = 0;
    config.debug = 0;
}



int main (int argc, char **argv) {
    char *configFile;
    int sockfd, tunfd;
    
    signal(SIGTERM, handler_term);
    signal(SIGINT, handler_term);
    

    if (parse_args(argc, argv, &configFile) != 0) {
        usage();
    }
    
    if (config.daemonize) daemonize();
    
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
        printf("  Timeout: %d sec.\n\n", config.timeout);
    }
    
    sockfd = create_socket();
    if (sockfd < 0) {
        exit(1);
    }
    
    /* The UDP socket is configured
     * Now, register to the rendezvous server
     */
    if (register_rdv(sockfd)) {
        exit(1);
    }

    tunfd = init_tun(1);
    if (tunfd < 0) {
        exit(1);
    }
    
    puts("Starting VPN");
    
    start_vpn(sockfd, tunfd);
    
    close_tun(tunfd);
    close(sockfd);
    return 0;
}

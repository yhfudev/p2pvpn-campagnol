/*
 * Startup code
 *
 * Copyright (C) 2007 Antoine Vianey
 *               2008-2011 Florent Bondoux
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

#include <sys/ioctl.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <getopt.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <sys/mman.h>


#include "tun_device.h"
#include "net_socket.h"
#include "communication.h"
#include "configuration.h"
#include "dtls_utils.h"
#include "../common/log.h"
#include "../common/pthread_wrap.h"

volatile sig_atomic_t end_campagnol = 0;
volatile sig_atomic_t reload = 0;
static char* pidfile = NULL;


static void usage(void) {
    fprintf(stderr, "Usage: campagnol [OPTION]... [configuration_file]\n\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, " -d, --debug             debug mode\n");
    fprintf(stderr, " -D, --daemon            fork in background\n");
    fprintf(stderr, " -h, --help              this help message\n");
    fprintf(stderr, " -m, --mlock             lock the memory into RAM\n");
    fprintf(stderr, " -p, --pidfile=FILE      write the pid into this file when running in background\n");
    fprintf(stderr, " -v, --verbose           verbose mode\n");
    fprintf(stderr, " -V, --version           show version information and exit\n\n");
    fprintf(stderr, "If no configuration file is given, the default is " DEFAULT_CONF_FILE "\n");
    exit(EXIT_FAILURE);
}

static void version(void) {
    fprintf(stderr, "Campagnol VPN | Client | Version %s\n", VERSION);
    fprintf(stderr, "Copyright (c) 2007 Antoine Vianey\n");
    fprintf(stderr, "              2008-2011 Florent Bondoux\n");
    exit(EXIT_SUCCESS);
}

static int parse_args(int argc, char **argv, const char **configFile) {
    int opt;

    struct option long_options[] = {
        {"verbose", 0, NULL, 'v'},
        {"daemon", 0, NULL, 'D'},
        {"debug", 0, NULL, 'd'},
        {"version", 0, NULL, 'V'},
        {"help", 0, NULL, 'h'},
        {"mlock", 0, NULL, 'm'},
        {"pidfile", 1, NULL, 'p'},
        {0, 0, 0, 0}
    };
    while ((opt = getopt_long(argc, argv, "vVdDhmp:", long_options, NULL)) >= 0) {
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
            case 'm' :
#ifdef HAVE_MLOCKALL
                if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
                    log_error(errno, "Unable to lock the memory");
                    exit(EXIT_FAILURE);
                }
#else
                log_message("This platform doesn't support mlockall.");
#endif
                break;
            case 'p' :
                if (config.pidfile)
                    free(config.pidfile);
                config.pidfile = CHECK_ALLOC_FATAL(strdup(optarg));
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

static void remove_pidfile(void) {
    unlink(pidfile);
    free(pidfile);
}

static void create_pidfile(void) {
    int fd;
    FILE *file;

    if (strlen(pidfile) == 0)
        return;

    if (unlink(pidfile) != 0 && errno != ENOENT) {
        return;
    }
    fd = open(pidfile, O_CREAT|O_WRONLY|O_TRUNC|O_NOFOLLOW, (mode_t) 00644);
    if (fd == -1) {
        return;
    }
    file = fdopen(fd, "w");
    if (file == NULL) {
        return;
    }

    fprintf(file, "%d\n", getpid());

    fclose(file);

    atexit(remove_pidfile);
}

static void daemonize(void) {
    int r;

    printf("Going in background...\n");
    r = daemon(1, 0);
    if (r != 0) {
        log_error(errno, "Unable to daemonize");
        exit(EXIT_FAILURE);
    }

    config.verbose = 0;
    config.debug = 0;
}

/* thread used to cath and handle signals */
static void * sig_handler(void * arg __attribute__((unused))) {
    sigset_t mask;
    int sig;
    struct itimerval timer_ping;

    while (1) {
        sigfillset(&mask);
        sigwait(&mask, &sig);

        switch (sig) {
            case SIGTERM:
            case SIGINT:
            case SIGQUIT:
                end_campagnol = 1;
                // terminate timer
                timer_ping.it_interval.tv_sec = 0;
                timer_ping.it_interval.tv_usec = 0;
                timer_ping.it_value.tv_sec = 0;
                timer_ping.it_value.tv_usec = 0;
                setitimer(ITIMER_REAL, &timer_ping, NULL);
                log_message("Received signal %d, exiting...", sig);
                return NULL;
            case SIGALRM:
                handler_sigTimerPing(sig);
                break;
            case SIGUSR1:
                log_message("Received signal %d, reloading client...", sig);
                end_campagnol = 1;
                reload = 1;
                break;
            case SIGUSR2:
                if (!end_campagnol) {
                    log_message("Received signal %d, recreating DTLS contexts...", sig);
                    rebuildDTLS();
                }
                break;
            default:
                break;
        }
    }

}


int main (int argc, char **argv) {
    const char *configFile = NULL;
    int sockfd = 0, tunfd = 0;
    int pa;
    int exit_status = EXIT_SUCCESS;
    int send_bye = 0;
    int log_level = 0;

    initConfig();

    pa = parse_args(argc, argv, &configFile);
    if (pa == 1) {
        usage();
    }
    else if (pa == 2) {
        version();
    }

    if (config.debug)
        log_level = 2;
    else if (config.verbose)
        log_level = 1;
    else
        log_level = 0;

    if (config.daemonize) daemonize();
    log_init(config.daemonize, log_level, "campagnol");

    /* init openssl */
    SSL_library_init();
    SSL_load_error_strings();
    setup_openssl_thread();

    if (parseConfFile(configFile) != 0) {
        goto clean_end;
    }

    if (config.daemonize) {
        const char *pidtmp = (config.pidfile != NULL) ? config.pidfile : DEFAULT_PID_FILE;
        pidfile = CHECK_ALLOC_FATAL(strdup(pidtmp));
        create_pidfile();
    }

    if (config.verbose) {
        puts("Configuration:");
        printf("  Local IP address: %s\n", inet_ntoa (config.localIP));
        if (config.iface) printf("  Using interface: %s\n", config.iface);
        if (config.send_local_addr == 2) {
            printf("  Send this local address to the RDV server: %s %d\n",
                    inet_ntoa(config.override_local_addr.sin_addr),
                    ntohs(config.override_local_addr.sin_port));
        }
        if (config.localport != 0) printf("  Using local port: %u\n", config.localport);
        printf("  RDV server IP address: %s\n", inet_ntoa (config.serverAddr.sin_addr));
        printf("  RDV server port: %d\n", ntohs(config.serverAddr.sin_port));
        printf("  VPN IP addres: %s\n", inet_ntoa(config.vpnIP));
        printf("  VPN broadcast IP: %s\n", inet_ntoa(config.vpnBroadcastIP));
        printf("  VPN subnetwork: %s\n", config.network);
        printf("  DTLS certificate file: %s\n", config.certificate_pem);
        printf("  DTLS private key file: %s\n", config.key_pem);
        if (config.verif_pem != NULL) printf("  DTLS root certificates chain file: %s\n", config.verif_pem);
        if (config.verif_dir != NULL) printf("  DTLS root certificates directory: %s\n", config.verif_dir);
        if (config.cipher_list) printf("  DTLS cipher list: %s\n", config.cipher_list);
        if (config.crl != NULL) printf("  Using a certificate revocation list: %s\n", config.crl);
        printf("  FIFO size: %d\n", config.FIFO_size);
        if (config.tb_client_rate > 0) printf("  Outgoing traffic: %.3f kb/s\n", config.tb_client_rate);
        if (config.tb_connection_rate > 0) printf("  Outgoing traffic per connection: %.3f kb/s\n", config.tb_connection_rate);
        printf("  Timeout: %d sec.\n", config.timeout);
        printf("  Keepalive: %u sec.\n", config.keepalive);
        printf("  Maximum number of connections: %d\n\n", config.max_clients);
    }

    sockfd = create_socket();
    if (sockfd < 0) {
        exit_status = EXIT_FAILURE;
        goto clean_end;
    }
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
        log_error(errno, "Could not set non-blocking mode on the socket");
        exit_status = EXIT_FAILURE;
        goto clean_end;
    }


    /* mask all signals in this thread and child threads */
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    /* start the signal handler */
    createDetachedThread(sig_handler, NULL);

    tunfd = init_tun();
    if (tunfd < 0) {
        exit_status = EXIT_FAILURE;
        goto clean_end;
    }

    do {
        if (reload) {
            reload = 0;
            end_campagnol = 0;
        }

        log_message("Starting VPN");

        if (start_vpn(sockfd, tunfd) == -1) {
            send_bye = 1;
            exit_status = EXIT_FAILURE;
        }

    }
    while(reload);


    clean_end:

    if (send_bye) {
        message_t smsg;
        smsg.ip1.s_addr = config.vpnIP.s_addr;
        smsg.ip2.s_addr = 0;
        smsg.port = 0;
        smsg.type = BYE;
        log_message_level(2, "Sending BYE");
        xsendto(sockfd,&smsg,sizeof(smsg),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
    }

    if (tunfd > 0)
        close_tun(tunfd);
    if (sockfd > 0)
        close(sockfd);

    log_close();

    /* try to clean up everything in openssl
     * OpenSSL has no cleanup function
     * AFAIK there is nothing to cleanup the compression methods
     */
    // free the strings stored in config
    freeConfig();
    // thread error state. must be called by each thread
    SSL_REMOVE_ERROR_STATE;
    cleanup_openssl_thread();
    // engine
    ENGINE_cleanup();
    CONF_modules_free();

    ERR_free_strings();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();

    exit(exit_status);
}

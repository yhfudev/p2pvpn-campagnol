/*
 * Campagnol RDV server, startup code
 *
 * Copyright (C) 2009-2011 Florent Bondoux
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

#include "rdv.h"

#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

#include "../common/log.h"
#include "net_socket.h"
#include "server.h"

struct configuration config;
static char* pidfile = NULL;

volatile sig_atomic_t end_server = 0;

static void usage(void) {
    fprintf(stderr, "Usage: campagnol_rdv [OPTION]...\n\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, " -D, --daemon            fork in background\n");
    fprintf(stderr, " -d, --debug             debug mode. Can be use twice to dump the packets\n");
    fprintf(stderr, " -h, --help              this help message\n");
    fprintf(stderr, " -m, --max-clients=N     set the maximum number of connected clients\n");
    fprintf(stderr, " -P, --pidfile=FILE      write the pid into this file when running in background\n");
    fprintf(stderr, " -p, --port=PORT         listening port\n");
    fprintf(stderr, " -v, --verbose           verbose mode\n");
    fprintf(stderr, " -V, --version           show version information and exit\n\n");
    exit(EXIT_FAILURE);
}

static void version(void) {
    fprintf(stderr, "Campagnol VPN | Server | Version %s\n", VERSION);
    fprintf(stderr, "Copyright (c) 2007 Antoine Vianey\n");
    fprintf(stderr, "              2008-2011 Florent Bondoux\n");
    exit(EXIT_SUCCESS);
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

static void sig_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            end_server = 1;
            log_message("Received signal %d, exiting...", sig);
            break;
        default:
            break;
    }
}

static int parse_args(int argc, char **argv) {
    int opt;
    int i;
    char *endptr;

    config.verbose = 0;
    config.daemonize = 0;
    config.debug = 0;
    config.dump = 0;
    config.serverport = SERVER_PORT_DEFAULT;
    config.max_clients = MAX_CLIENTS_DEFAULT;
    config.pidfile = NULL;

    struct option long_options[] = {
        {"daemon", 0, NULL, 'D'},
        {"debug", 0, NULL, 'd'},
        {"help", 0, NULL, 'h'},
        {"max-clients", 1, NULL, 'm'},
        {"pidfile", 1, NULL, 'P'},
        {"port", 1, NULL, 'p'},
        {"verbose", 0, NULL, 'v'},
        {"version", 0, NULL, 'V'},
        {0, 0, 0, 0}
    };
    while ((opt = getopt_long(argc, argv, "dDhm:p:P:vV", long_options, NULL)) >= 0) {
        switch(opt) {
            case 'D' :
                config.daemonize = 1;
                break;
            case 'd' :
                if (config.debug) {
                    config.dump = 1;
                }
                else {
                    config.debug = 1;
                    config.verbose = 1;
                }
                break;
            case 'h' :
                return 1;
            case 'm':
                errno = 0;
                config.max_clients = (int) strtol(optarg, &endptr, 10);
                if (errno != 0 || endptr == optarg || config.max_clients < 0) {
                    return 1;
                }
                break;
            case 'p' :
                i = atoi(optarg);
                if (i <= 0 || i > UINT16_MAX) {
                    return 1;
                }
                config.serverport = (uint16_t) i;
                break;
            case 'P' :
                if (config.pidfile)
                    free(config.pidfile);
                config.pidfile = CHECK_ALLOC_FATAL(strdup(optarg));
                break;
            case 'v':
                config.verbose = 1;
                break;
            case 'V' :
                return 2;

            default : return 1;
        }
    }

    argv += optind;
    argc -= optind;

    if (argc != 0) {
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    int pa;
    int sockfd;
    int exit_status = EXIT_SUCCESS;
    int log_level;

    pa = parse_args(argc, argv);
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
    log_init(config.daemonize, log_level, "campagnol_rdv");

    if (config.daemonize) {
        const char *pidtmp = (config.pidfile != NULL) ? config.pidfile : DEFAULT_PID_FILE;
        pidfile = CHECK_ALLOC_FATAL(strdup(pidtmp));
        create_pidfile();
    }

    sockfd = create_socket();
    if (sockfd < 0) {
        exit_status = EXIT_FAILURE;
        goto clean_end;
    }

    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);

    rdv_server(sockfd);

    clean_end:

    if (sockfd > 0)
        close(sockfd);

    log_close();

    if (config.pidfile)
        free(config.pidfile);

    exit(exit_status);
}

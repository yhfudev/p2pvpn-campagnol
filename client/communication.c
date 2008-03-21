/*
 * Campagnol main code
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


#include <time.h> 
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "campagnol.h"
#include "communication.h"
#include "pthread_wrap.h"
#include "net_socket.h"
#include "peer.h"


/** affiche le message a l ecran */
void print_smsg(struct message *smsg) {
    int i, k;
    char *s;
    switch(smsg->type) {
        case HELLO :            s = "| HELLO "; break;
        case PING :             s = "| PING  "; break;
        case ASK_CONNECTION :   s = "| ASK   "; break;
        case PONG :             s = "| PONG  "; break;
        case OK :               s = "| OK    "; break;
        case NOK :              s = "| NOK   "; break;
        case FWD_CONNECTION :   s = "| FWD   "; break;
        case ANS_CONNECTION :   s = "| ANS   "; break;
        case REJ_CONNECTION :   s = "| REJ   "; break;
        case PUNCH :            s = "| PUNCH "; break;
        case PUNCH_ACK :        s = "| PEER+ "; break;
        case BYE :              s = "| BYE   "; break;
        case RECONNECT :        s = "| RECNCT"; break;
        case CLOSE_CONNECTION :            s = "| CLOSE "; break;
        default : return; break;
    }

    printf("*****************************************************\n"
           "| TYPE  | PORT  | IP 1            | IP 2            |\n");
    printf("%s", s);
    // port
    k = ntohs(smsg->port);
    printf("| %-6d", k);
    // ip1
    char *tmp = inet_ntoa(smsg->ip1);
    printf("| %s", tmp);
    for (i=0; i<16-strlen(tmp); i++) printf(" ");
    // ip2
    tmp = inet_ntoa(smsg->ip2);
    printf("| %s", tmp);
    for (i=0; i<16-strlen(tmp); i++) printf(" ");
    printf("|\n*****************************************************\n");
} 
/** fin de procedure */

/** initialise le message */
void init_smsg(struct message *smsg, int type, u_int32_t ip1, u_int32_t ip2) {
    bzero(smsg, sizeof(struct message));
    smsg->type = type;
    smsg->ip1.s_addr = ip1;
    smsg->ip2.s_addr = ip2;
}
/** fin de procédure */

/** procedure initialisant le temp d attente du select */
void init_timeout(struct timeval *timeout) {
    timeout->tv_sec = ATTENTE_SECONDES;
    timeout->tv_usec = ATTENTE_MICROSECONDES;
}
/** fin de procedure */


/* Calcul de checksum
 * voir RFC1071
 * http://tools.ietf.org/html/rfc1071
 * modification de l'exemple donné
 * fonction utilisée pour modifier les adresses broadcast
 */
uint16_t compute_csum(uint16_t *addr, size_t count){
   register int32_t sum = 0;

    while( count > 1 )  {
        /*  This is the inner loop */
        sum += * addr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if( count > 0 )
        sum += (* (unsigned char *) addr) << 8;

    /*  Fold 32-bit sum to 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);

    return sum;
}


/** Enregistrement auprès du serveur de rendez-vous
 * sockfd : socket
 * serveraddr : adresse serveur RDV
 * vpnIP : adresse VPN du client
 */
int register_rdv(int sockfd) {
    struct message smsg;
    struct timeval timeout;
    
    int s,r;
    int notRegistered = 1;
    int registeringTries = 0;
    fd_set fd_select;
    
    /** initialisation du message HELLO */  
    if (config.verbose) printf("Enregistrement auprès du serveur...\n");
    init_smsg(&smsg, HELLO, config.vpnIP.s_addr, 0);
    if (config.debug) printf("Sending HELLO");
    
    while (notRegistered && registeringTries<MAX_REGISTERING_TRIES && !end_campagnol) {
        /** envoi du HELLO au server */
        registeringTries++;
        if ((s=sendto(sockfd,&smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr))) == -1) {
            perror("sendto");
        }
        if (config.debug) printf(".");
        fflush(stdout);
        
        /** initialisation du select */
        FD_ZERO(&fd_select);
        FD_SET(sockfd, &fd_select);
        init_timeout(&timeout);
        select(sockfd+1, &fd_select, NULL, NULL, &timeout);
        
        if (FD_ISSET(sockfd, &fd_select)!=0 && !end_campagnol) {
            if (config.debug) printf("\n");
            /** message reçu du serveur */
            socklen_t len = sizeof(struct sockaddr_in);
            if ( (r = recvfrom(sockfd,&smsg,MESSAGE_MAX_LENGTH,0,(struct sockaddr *)&config.serverAddr,&len)) == -1) {
                perror("recvfrom");
            }
            switch (smsg.type) {
                case OK:
                    /** on est accepte dans le VPN */
                    if (config.verbose) puts("\nOn est maintenant connecté au VPN");
                    notRegistered = 0;
                    return 0;
                    break;
                case NOK:
                    /** on est rejeté du VPN */
                    fprintf(stderr, "\nLe serveur de rendez-vous rejete la demande de connexion\n");
                    return 1;
                    break;
                default:
                    fprintf(stderr, "\nReçu un message autre que OK ou NOK : %d\n", smsg.type);
                    break;
            }
        }
    }

    /** La connexion à echouée le serveur est down */
    if (notRegistered) {
        fprintf(stderr, "\nLa connexion au serveur a échouée\n");
        return 1;
    }
    return 0;
}


/** procédure pour le punch des clients distants */
void *punch(void *arg) {
    int i;
    struct punch_arg *str = (struct punch_arg *)arg;
    struct message smsg;
    init_smsg(&smsg, PUNCH, config.vpnIP.s_addr, 0);
    str->peer->time = time(NULL);
    /** on punch le client */
    MUTEXLOCK;
    if (str->peer->state!=ESTABLISHED) {
        str->peer->state = PUNCHING;
    }
    MUTEXUNLOCK;
    if (config.verbose) printf("punch %s %d\n", inet_ntoa(str->peer->clientaddr.sin_addr), ntohs(str->peer->clientaddr.sin_port));
    for (i=0; i<NOMBRE_DE_PUNCH; i++) {
        sendto(*(str->sockfd),&smsg,sizeof(struct message),0,(struct sockaddr *)&(str->peer->clientaddr), sizeof(str->peer->clientaddr));
        usleep(PAUSE_ENTRE_PUNCH);
    }
    /** on attend une réponse du peer */
    MUTEXLOCK;
    if (str->peer->state!=ESTABLISHED) {
        str->peer->state = WAITING;
    }
    MUTEXUNLOCK;
    free(arg);
    pthread_exit(NULL);
}
/** fin de procédure */

/** procédure appelée pour lancer correctement le thread de punch */
void start_punch(struct client *peer, int *sockfd) {
    /** structure pour rassembler les argument du thread */
    struct punch_arg *arg = malloc(sizeof(struct punch_arg));
    arg->sockfd = sockfd;
    arg->peer = peer;
    /** creation effective du thread */
    pthread_t t;
    t = createThread(punch, (void *)arg);
}
/** fin de procédure */

/* Thread associé à chaque flux SSL
 * 
 * Effectue la connexion SSL, 
 * puis boucle en lecture sur SSL_read
 * 
 * le flux SSL est "alimenté" par les appels à BIO_write sur peer->rbio
 */
void * lectureSSL(void * args) {
    int r;
    union {                                     // structure de reception des messages TUN
        struct {
            struct iphdr ip;
        } fmt;
        unsigned char raw[MESSAGE_MAX_LENGTH]; //65536
    } u; 
    struct client *peer = (struct client*) args;
    int tunfd = peer->tunfd;
    
    // Connexion
    r = peer->ssl->handshake_func(peer->ssl);
    if (r != 1) {
        fprintf(stderr, "Erreur de connexion\n");
        ERR_print_errors_fp(stderr);
        MUTEXLOCK;
        remove_client(peer);
        MUTEXUNLOCK;
        struct message smsg;
        init_smsg(&smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
        sendto(peer->sockfd, &smsg, sizeof(struct message), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
        conditionSignal(&peer->cond_connected);
        return NULL;
    }
    // Déblocage du thread en attente
    conditionSignal(&peer->cond_connected);
    
    while (!end_campagnol) {
        // Lecture SSL puis envoi sur tun
        
        r = SSL_read(peer->ssl, &u, MESSAGE_MAX_LENGTH);
        if (r < 0) { // erreur
            int err = SSL_get_error(peer->ssl, r);
            fprintf(stderr, "SSL_read %d\n", err);
            ERR_print_errors_fp(stderr);
        }
        MUTEXLOCK;
        if (r == 0) { // fin connexion
            if (config.verbose) printf("connexion ssl fermée avec %s\n", inet_ntoa(peer->vpnIP));
            if (end_campagnol) {
                MUTEXUNLOCK;
                return NULL;
            }
            peer->state = TIMEOUT;
            peer->thread_running = 0;
            //createClientSSL(peer, 1); // les structures SSL sont réinitialisées
            remove_client(peer);
            MUTEXUNLOCK;
            return NULL;
        }
        else {// lecture ok
            peer->time = time(NULL);
            peer->state = ESTABLISHED;
            MUTEXUNLOCK;
            if (config.debug) printf("...\nMessage VPN reçu de taille %d de SRC = %u.%u.%u.%u pour DST = %u.%u.%u.%u\n",
                            r,
                            (ntohl(u.fmt.ip.saddr) >> 24) & 0xFF,
                            (ntohl(u.fmt.ip.saddr) >> 16) & 0xFF,
                            (ntohl(u.fmt.ip.saddr) >> 8) & 0xFF,
                            (ntohl(u.fmt.ip.saddr) >> 0) & 0xFF,
                            (ntohl(u.fmt.ip.daddr) >> 24) & 0xFF,
                            (ntohl(u.fmt.ip.daddr) >> 16) & 0xFF,
                            (ntohl(u.fmt.ip.daddr) >> 8) & 0xFF,
                            (ntohl(u.fmt.ip.daddr) >> 0) & 0xFF);
            // passage à la couche tun
            write(tunfd, (unsigned char *)&u, sizeof(u));
        }
    }
    return NULL;
}

/* Démarrage du thread lectureSSL
 */
void createSSL(struct client *peer) {
    peer->thread_running = 1;
    peer->thread = createThread(lectureSSL, peer);
}


/** Gestion des communications entrant sur la socket UDP
 * argument : struct comm_args *
 */
void * comm_socket(void * argument) {
    struct comm_args * args = argument;
    int sockfd = args->sockfd;
    int tunfd = args->tunfd;
    
    int r;
    int r_select;
    fd_set fd_select;                           // configuration du select
    struct timeval timeout;                     // timeout lecture sur la socket
    union {                                     // structure de reception des messages TUN
        struct {
            struct iphdr ip;
        } fmt;
        unsigned char raw[MESSAGE_MAX_LENGTH]; //65536
    } u;                                        // lecture sur la socket UDP
    struct sockaddr_in unknownaddr;             // découverte de l'adresse émétrice
    //struct in_addr dest;                        // adresse envoi
    socklen_t len = sizeof(struct sockaddr_in);
    struct message *rmsg = (struct message *) &u;   // message lu 
    struct message *smsg = (struct message *) malloc(sizeof(struct message)); // message à envoyer
    struct client *peer;                        // reference vers un client du vpn
    
    init_timeout(&timeout);
    
    /** on rentre dans la boucle du programme */
    while (!end_campagnol) {
        /** Initialisation du select */
        FD_ZERO(&fd_select);
        FD_SET(sockfd, &fd_select);     // enregistrement de la socket
        r_select = select(sockfd+1, &fd_select, NULL, NULL, &timeout);
    
        /** MESSAGE RECU SUR LA SOCKET */
        if (r_select > 0) {
            r = recvfrom(sockfd,(unsigned char *)&u,MESSAGE_MAX_LENGTH,0,(struct sockaddr *)&unknownaddr,&len);
            /** Message du serveur */
            if (config.serverAddr.sin_addr.s_addr == unknownaddr.sin_addr.s_addr
                && config.serverAddr.sin_port == unknownaddr.sin_port) {
                /** On analyse le message du serveur */
                switch (rmsg->type) {
                    /** Réception d'un rejet du serveur */
                    case REJ_CONNECTION :
                        MUTEXLOCK;
                        peer = get_client_VPN(&(rmsg->ip1));
                        /** le client n'est pas connecté */
                        //peer->state = NOT_CONNECTED;
                        remove_client(peer);
                        MUTEXUNLOCK;
                        break;
                    /** Réception d'une réponse à une demande de connexion à un peer */
                    case ANS_CONNECTION :
                        /** Récuperation du client */
                        MUTEXLOCK;
                        peer = get_client_VPN(&(rmsg->ip2));
                        /** On complète ses informations */
                        peer->state = PUNCHING;
                        peer->clientaddr.sin_addr = rmsg->ip1;
                        peer->clientaddr.sin_port = rmsg->port;
                        peer->time = time(NULL);
                        /** on lance le punch */
                        MUTEXUNLOCK;
                        start_punch(peer, &sockfd);
                        break;
                    /** Réception d'une demande de connexion d'un peer */
                    case FWD_CONNECTION :
                        MUTEXLOCK;
                        if (get_client_VPN(&(rmsg->ip2)) == NULL) {
                            /** Il faut creer la structure */
                            peer = add_client(sockfd, tunfd, PUNCHING, time(NULL), rmsg->ip1, rmsg->port, rmsg->ip2, 0);
                        }
                        /** On complète ses informations */
                        else {
                            peer = get_client_VPN(&(rmsg->ip2));
                            peer->state = PUNCHING;
                            peer->clientaddr.sin_addr = rmsg->ip1;
                            peer->clientaddr.sin_port = rmsg->port;
                            peer->time = time(NULL);
                        }
                        MUTEXUNLOCK;
                        /** on lance le punch */
                        start_punch(peer, &sockfd);
                        break;
                    /** Réception d'une demande de reconnexion auprès du serveur de RDV
                     * (le serveur a redémarré)
                     */
                    case RECONNECT :
                        register_rdv(sockfd);
                        break;
                    /** Réception d'un PONG depuis le serveur */
                    case PONG :
                        /** le serveur est toujours vivant */
                        break;
                    default :
                        break;
                }
            }
            /** Message d'un client */
            else {
                // voir la structure de l'entete IP qui permettrait de différencier les types de messages
                if (config.debug) printf("Paquet reçu d'un client du VPN de taille %d\n", r);
                /** Le paquet est un PUNCH ou un PUNCH_ACK */
                if (rmsg->type == PUNCH || rmsg->type == PUNCH_ACK) {
                    /** On analyse le message du serveur */
                    switch (rmsg->type) {
                        /** Réception d'un punch d'un client */
                        case PUNCH :
                        case PUNCH_ACK :
                            /** le client est dorénavant joignable sur l'adresse et le port d'émission du punch */
                            MUTEXLOCK;
                            peer = get_client_VPN(&(rmsg->ip1));
                            if (peer != NULL) { // TODO : cas d'un punch qui arrive avant un FWD_CONNECTION
                                if (config.verbose && peer->state != ESTABLISHED) printf("connecté à %s\n", inet_ntoa(rmsg->ip1));
                                peer->time = time(NULL);
                                peer->state = ESTABLISHED;
                                peer->clientaddr = unknownaddr;
                                if (peer->thread_running == 0) {
                                    BIO_ctrl(peer->wbio, BIO_CTRL_DGRAM_SET_PEER, 0, &peer->clientaddr);
                                    createSSL(peer);
                                }
                            }
                            MUTEXUNLOCK;
                            break;
                        default :
                            break;
                    }
                }
                /** Le paquet contient un paquet IP à transmettre à la couche TUN */
                else {
                    MUTEXLOCK;
                    peer = get_client_real(&unknownaddr);
                    if (peer != NULL) { // sinon : paquet non voulu et crash
                        // débloquer absolument avant l'écriture pour permettre
                        // un appel simultanné de SSL_read
                        MUTEXUNLOCK;  
                        BIO_write(peer->rbio, &u, r);
                    }
                    else {
                        MUTEXUNLOCK;
                    }
                    
                }
            }
            
        
        }
        
        
        /** TIMEOUT */
        else if (r_select == 0) {
            /** envoi d'un ping au serveur */
            init_smsg(smsg, PING,0,0);
            init_timeout(&timeout);
            int s = sendto(sockfd,smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
            if (s == -1) perror("PING");
            /** verifier l'etat des clients en cours */
            MUTEXLOCK;
            peer = clients;
            while (peer != NULL) {
                struct client *next = peer->next;
                if (time(NULL)-peer->time>config.timeout) {
                    peer->state = TIMEOUT;
                    if (peer->ssl != NULL) {
                        if (config.debug) printf("peer timeout %s\n", inet_ntoa(peer->vpnIP));                      
                        if (peer->is_dtls_client && peer->thread_running) {// Fermeture des connexions que l'on a initié
                            peer->thread_running = 0;
                            SSL_shutdown(peer->ssl);
                            BIO_write(peer->rbio, &u, 0);
                            init_smsg(smsg, CLOSE_CONNECTION, peer->vpnIP.s_addr, 0);
                            s = sendto(sockfd, smsg, sizeof(struct message), 0, (struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                        }
                    }
                }
                peer = next;
            }
            MUTEXUNLOCK;
        }
    
    
    }
    
    free(smsg);
    return NULL;
}


/** Gestion de la communication entrant sur le driver tun
 * argument : struct comm_args *
 */
void * comm_tun(void * argument) {
    struct comm_args * args = argument;
    int sockfd = args->sockfd;
    int tunfd = args->tunfd;
    
    int r;
    int r_select;
    fd_set fd_select;                           // configuration du select
    struct timeval timeout;                     // timeout lecture sur tun
    struct timespec timeout_connect;            // timeout pour attente de connexion
    union {                                     // structure de reception des messages TUN
        struct {
            struct iphdr ip;
        } fmt;
        unsigned char raw[MESSAGE_MAX_LENGTH]; //65536
    } u;                                        // message lu sur tun. TODO MAX_LENGTH réelle = MTU
    struct in_addr dest;
    struct message *smsg = (struct message *) malloc(sizeof(struct message));
    struct client *peer;                        // reference vers un client du vpn


    /** on rentre dans la boucle du programme */
    while (!end_campagnol) {
        /** Initialisation du select */
        init_timeout(&timeout);
        FD_ZERO(&fd_select);
        FD_SET(tunfd, &fd_select);      // enregistrement de TUN
        r_select = select(tunfd+1, &fd_select, NULL, NULL, &timeout);
        
        
        /** deuxième possibilité : MESSAGE RECU SUR L'INTERFACE TUN */
        if (r_select > 0) {
            /** Lecture du paquet IP */
            r = read(tunfd, &u, sizeof(u));
            /** Affichage des informations concernant le paquet reçu */
            if (config.debug) printf("...\nMessage à envoyer sur le VPN de taille %d de SRC = %u.%u.%u.%u  pour DST = %u.%u.%u.%u\n",
                                r,
                                (ntohl(u.fmt.ip.saddr) >> 24) & 0xFF,
                                (ntohl(u.fmt.ip.saddr) >> 16) & 0xFF,
                                (ntohl(u.fmt.ip.saddr) >> 8) & 0xFF,
                                (ntohl(u.fmt.ip.saddr) >> 0) & 0xFF,
                                (ntohl(u.fmt.ip.daddr) >> 24) & 0xFF,
                                (ntohl(u.fmt.ip.daddr) >> 16) & 0xFF,
                                (ntohl(u.fmt.ip.daddr) >> 8) & 0xFF,
                                (ntohl(u.fmt.ip.daddr) >> 0) & 0xFF);
            dest.s_addr = u.fmt.ip.daddr;

            /* IP dest = ip broadcast du VPN 
             * en réception, la couche TUN est point à point, 
             * donc elle ne transmet pas l'IP broadcast
             * modification de l'IP destinataire vers l'IP VPN normale, 
             * puis recalcul du checksum IP
             */
            if (dest.s_addr == config.vpnBroadcastIP.s_addr) {
                MUTEXLOCK;
                peer = clients;
                while (peer != NULL) {
                    struct client *next = peer->next;
                    if (peer->state == ESTABLISHED) {
                        u.fmt.ip.daddr = peer->vpnIP.s_addr;
                        u.fmt.ip.check = 0; // champ checksum à 0 pour le calcul
                        u.fmt.ip.check = ~compute_csum((uint16_t*) &u.fmt.ip, sizeof(u.fmt.ip));
                        SSL_write(peer->ssl, &u, r);
                    }
                    peer = next;
                }
                MUTEXUNLOCK;
            }
            else {
                MUTEXLOCK;
                peer = get_client_VPN(&dest);
                MUTEXUNLOCK;
                if (peer == NULL) {// état UNKNOWN
                    MUTEXLOCK;
                    peer = add_client(sockfd, tunfd, TIMEOUT, time(NULL), (struct in_addr) { 0 }, 0, dest, 1);
                    MUTEXUNLOCK;
                    /** Envoi d'une demande d'information au serveur */
                    init_smsg(smsg, ASK_CONNECTION, dest.s_addr, 0);
                    sendto(sockfd,smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                    
                    clock_gettime(CLOCK_REALTIME, &timeout_connect);
                    timeout_connect.tv_sec += 3;
                    MUTEXLOCK;
                    if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                        peer = get_client_VPN(&dest);
                        if (peer != NULL) // la connexion a réussi
                            SSL_write(peer->ssl, &u, r);
                    }
                    MUTEXUNLOCK;
                    
                }
                else switch (peer->state) {
                    /** Le client est connecté */
                    case ESTABLISHED :
                        /** Récupération du client */
                        SSL_write(peer->ssl, &u, r);
                        break;
                    /** Le client est anciennement connu */
                    case TIMEOUT :
                        /** on sait jamais le message peut être reçu tout de même */
                        MUTEXLOCK;
                        peer->time = time(NULL);
                        MUTEXUNLOCK;
                        /** Envoi d'une demande d'information au serveur */
                        init_smsg(smsg, ASK_CONNECTION, dest.s_addr, 0);
                        sendto(sockfd,smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr));
                        
                        clock_gettime(CLOCK_REALTIME, &timeout_connect);
                        timeout_connect.tv_sec += 3;
                        MUTEXLOCK;
                        if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                            peer = get_client_VPN(&dest);
                            if (peer != NULL) // la connexion a réussi
                                SSL_write(peer->ssl, &u, r);
                        }
                        MUTEXUNLOCK;
                        break;
                    /** Le client n'est pas connecté mais la structure existe pour ce client */
                    case NOT_CONNECTED :
                        if (time(NULL)-peer->time>config.timeout) {
                            MUTEXLOCK;
                            peer->state = TIMEOUT;
                            MUTEXUNLOCK;
                            // relancer le punch
                        }
                        break;
                    /** Le client est en cours de connexion */
                    default :
                        /** On est en train de puncher on peut toujours essayer */
                        clock_gettime(CLOCK_REALTIME, &timeout_connect);
                        timeout_connect.tv_sec += 2;
                        MUTEXLOCK;
                        if (conditionTimedwait(&peer->cond_connected, &mutex_clients, &timeout_connect) == 0) {
                            peer = get_client_VPN(&dest);
                            if (peer != NULL) // la connexion a réussi
                                SSL_write(peer->ssl, &u, r);
                        }
                        MUTEXUNLOCK;
                        break;
                }
            }
        }
    
    }
    
    free(smsg);
    return NULL;
}


/** Démarrer le VPN :
 * lance un thread sur comm_tun et un sur comm_socket
 * 
 * positionner end_campagnol pour arrêter les threads et retourner
 */
void lancer_vpn(int sockfd, int tunfd) {
    struct message smsg;
    struct comm_args *args = (struct comm_args*) malloc(sizeof(struct comm_args));
    args->sockfd = sockfd;
    args->tunfd = tunfd;
    
    SSL_library_init();
    SSL_load_error_strings();
    
    pthread_t th_socket, th_tun;
    th_socket = createThread(comm_socket, args);
    th_tun = createThread(comm_tun, args); // réutilisation des arguments
    
    pthread_join(th_socket, NULL);
    pthread_join(th_tun, NULL);
    
    free(args);
    
    struct client *peer, *next;
    peer = clients;
    while (peer != NULL) {
        next = peer->next;
        remove_client(peer); // attend la terminaison du thread de lecture SSL puis détruit
        peer = next;
    }
    
    
    init_smsg(&smsg, BYE, config.vpnIP.s_addr, 0);
    if (config.debug) printf("Sending BYE\n");
    if (sendto(sockfd,&smsg,sizeof(struct message),0,(struct sockaddr *)&config.serverAddr, sizeof(config.serverAddr)) == -1) {
        perror("sendto");
    }
}


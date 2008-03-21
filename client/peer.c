/*
 * Peers list management
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
  * Définition de la liste des clients et fonctions de manipulation :
  * ajout/suppression, recherche par IP/port réel ou par IP VPN
  */

#include "campagnol.h"
#include "peer.h"
#include "pthread_wrap.h"
#include "bss_fifo.h"

#include <arpa/inet.h>

// bibliothèque des clients connus
struct client *clients = NULL;
// Mutex pour manipuler la liste clients
pthread_mutex_t mutex_clients = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

/** Ajout d'un client à la liste clients */
struct client * add_client(int sockfd, int tunfd, int state, time_t time, struct in_addr clientIP, u_int16_t clientPort, struct in_addr vpnIP, int is_dtls_client) {
    struct client *peer = malloc(sizeof(struct client));
    peer->time = time;
    bzero(&(peer->clientaddr), sizeof(peer->clientaddr));
    peer->clientaddr.sin_family = AF_INET;
    peer->clientaddr.sin_addr = clientIP;
    peer->clientaddr.sin_port = clientPort;
    peer->vpnIP = vpnIP;
    peer->state = state;
    peer->tunfd = tunfd;
    peer->sockfd = sockfd;
    peer->cond_connected = createCondition();
    
    peer->is_dtls_client = is_dtls_client;
    peer->thread_running = 0;
    
    createClientSSL(peer, 0);
    
    peer->next = clients;
    peer->prev = NULL;
    if (peer->next) peer->next->prev = peer;
    clients = peer;
    if (config.debug) printf("Ajout structure pour le client %s\n", inet_ntoa(vpnIP));
    return peer;
}

/* Contient les deniers résultats de recherche avec
 * get_client_VPN et get_client_real
 */
struct client *cache_client_VPN = NULL;
struct client *cache_client_real = NULL;

/** Retire un client de la liste chaînée et désaloue les structures SSL associées
 */
void remove_client(struct client *peer) {
    if (config.debug) printf("Suppression structure pour le client %s\n", inet_ntoa(peer->vpnIP));
    
    if (peer->thread_running) {
        peer->thread_running = 0;
        SSL_shutdown(peer->ssl);
        BIO_write(peer->rbio, peer, 0);
        pthread_join(peer->thread, NULL);
    }
    
    destroyCondition(&peer->cond_connected);
    SSL_free(peer->ssl);
    SSL_CTX_free(peer->ctx);
    
    if (peer->next) peer->next->prev = peer->prev;
    if (peer->prev) {
        peer->prev->next = peer->next;
    }
    else {
        clients = peer->next;
    }
    
    free(peer);
    cache_client_VPN = NULL;
    cache_client_real = NULL;
}

/** Retourne un client par son addresse VPN
 * ou NULL si le client est inconnu
 */
struct client * get_client_VPN(struct in_addr *address) {
    if (cache_client_VPN != NULL
        && cache_client_VPN->vpnIP.s_addr == address->s_addr) {
        return cache_client_VPN;
    }
    if (config.debug) printf("get %s\n", inet_ntoa(*address));
    struct client *peer = clients;
    while (peer != NULL) {
        if (peer->vpnIP.s_addr == address->s_addr) {
            /** le client est deja connu */
            cache_client_VPN = peer;
            return peer;
        }
        peer = peer->next;
    }
    return NULL;
}

/** Retourne un client par son addresse IP et port réels
 * ou NULL si le client est inconnu
 */
struct client * get_client_real(struct sockaddr_in *cl_address) {
    if (cache_client_real != NULL
        && cache_client_real->clientaddr.sin_addr.s_addr == cl_address->sin_addr.s_addr
        && cache_client_real->clientaddr.sin_port == cl_address->sin_port) {
        return cache_client_real;
    }
    if (config.debug) printf("get by client IP %s\n", inet_ntoa(cl_address->sin_addr));
    struct client *peer = clients;
    while (peer != NULL) {
        if (peer->clientaddr.sin_addr.s_addr == cl_address->sin_addr.s_addr
            && peer->clientaddr.sin_port == cl_address->sin_port) {
            /** le client est deja connu */
            cache_client_real = peer;
            return peer;
        }
        peer = peer->next;
    }
    return NULL;
}

/* Fonction callback utilisée pour vérifier si un certificat
 * est dans la CRL
 * un callback transparent se contente de retourner preverify_ok
 * si preverify_ok == 0, on sort
 */
int verify_crl(int preverify_ok, X509_STORE_CTX *x509_ctx) {
    X509_REVOKED *revoked;
    int i, n;
    
    if (!preverify_ok)
        return preverify_ok;
    
    if (config.crl) {
        // Vérification du nom du fournisseur du certificat
        if (X509_NAME_cmp(X509_CRL_get_issuer(config.crl), X509_get_issuer_name(x509_ctx->current_cert)) != 0) {
            fprintf(stderr, "Le certificat n'est pas un certificat de Campagnol\n");
            ERR_print_errors_fp(stderr);
            return 0;
        }
        
        // nombre de certificats dans la CRL
        n = sk_num(X509_CRL_get_REVOKED(config.crl));
        
        // Vérification pour chaque certificat dans la CRL, comparaison des numéros de série
        for (i = 0; i < n; i++) {
            revoked = (X509_REVOKED *)sk_value(X509_CRL_get_REVOKED(config.crl), i);
            if (ASN1_INTEGER_cmp(revoked->serialNumber, X509_get_serialNumber(x509_ctx->current_cert)) == 0) {
              fprintf(stderr, "Le certificat reçu est révoqué\n");
              ERR_print_errors_fp(stderr);
              return 0;
            }
        }
    }
    
    return preverify_ok;
}

/* Construit les objets SSL associés à un client
 * si recreate, alors les objets sont reconstruits
 */
void createClientSSL(struct client *peer, int recreate) {
    if (recreate) {
        SSL_CTX_free(peer->ctx);
        SSL_free(peer->ssl);
    }
    
    SSL_METHOD *meth = peer->is_dtls_client ? DTLSv1_client_method() : DTLSv1_server_method();
    peer->ctx = SSL_CTX_new(meth);
    
    if (!SSL_CTX_use_certificate_chain_file(peer->ctx, config.certificate_pem)) {
        perror("SSL_CTX_use_certificate_chain_file");
        fprintf(stderr, "%s\n", config.certificate_pem);
        exit(1);
    }
    if (!SSL_CTX_use_PrivateKey_file(peer->ctx, config.key_pem, SSL_FILETYPE_PEM)) {
        perror("SSL_CTX_use_PrivateKey_file");
        exit(1);
    }
    SSL_CTX_set_verify(peer->ctx, SSL_VERIFY_PEER, verify_crl);
    SSL_CTX_set_verify_depth(peer->ctx, 1);
    if (!SSL_CTX_load_verify_locations(peer->ctx, config.verif_pem, NULL)) {
        perror("SSL_CTX_load_verify_locations");
        exit(1);
    }
    
    // Nécessaire pour DTLS
    SSL_CTX_set_read_ahead(peer->ctx, 1);
    
    peer->ssl = SSL_new(peer->ctx);
    peer->wbio = BIO_new_dgram(peer->sockfd, BIO_NOCLOSE);
    peer->rbio = BIO_new(BIO_s_fifo());
    SSL_set_bio(peer->ssl, peer->rbio, peer->wbio);
    
    // Pas de compression
    peer->ssl->ctx->comp_methods = NULL;
    // Algorithmes
    if (strlen(config.cipher_list) != 0) {
        if (! SSL_CTX_set_cipher_list(peer->ssl->ctx, config.cipher_list)){
            perror("SSL_CTX_set_cipher_list");
            ERR_print_errors_fp(stderr);
            exit(1);
        }
    }
    peer->is_dtls_client ? SSL_set_connect_state(peer->ssl) : SSL_set_accept_state(peer->ssl);
    // Forcer la mtu à 1500
    SSL_set_options(peer->ssl, SSL_OP_NO_QUERY_MTU);
    peer->ssl->d1->mtu = 1500;
}

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
 * Definition of the list of known clients and manipulation functions:
 * add/remove, search by IP/port or by VPN IP
 */
#include "campagnol.h"
#include "peer.h"
#include "pthread_wrap.h"
#include "bss_fifo.h"
#include "log.h"

#include <arpa/inet.h>

/* List of known clients */
struct client *clients = NULL;
int n_clients = 0;
/*
 * mutex used to manipulate 'clients'
 * It needs to be recursive since decr_ref may lock it
 */
pthread_mutex_t mutex_clients;
pthread_mutexattr_t attrs_mutex_clients;

void mutex_clients_init(void) {
    mutexattrInit(&attrs_mutex_clients);
    mutexattrSettype(&attrs_mutex_clients, PTHREAD_MUTEX_RECURSIVE);
    mutexInit(&mutex_clients, &attrs_mutex_clients);
}

void mutex_clients_destroy(void) {
    mutexDestroy(&mutex_clients);
    mutexattrDestroy(&attrs_mutex_clients);
}

/*
 * Safely increment the reference counter of peer
 */
void incr_ref(struct client *peer) {
    mutexLock(&peer->mutex_ref);
    peer->ref_count++;
//    fprintf(stderr, "incr %s [%d]\n", inet_ntoa(peer->vpnIP), peer->ref_count);
    mutexUnlock(&peer->mutex_ref);
}

/*
 * Safely decrement the reference counter of peer
 * If the count reaches 0, the peer is freed
 */
void decr_ref(struct client *peer) {
    mutexLock(&peer->mutex_ref);
    peer->ref_count--;
//    fprintf(stderr, "decr %s [%d]\n", inet_ntoa(peer->vpnIP), peer->ref_count);
    if (peer->ref_count == 0) {
        mutexUnlock(&peer->mutex_ref);
        MUTEXLOCK;
        remove_client(peer);
        MUTEXUNLOCK;
    }
    else {
        mutexUnlock(&peer->mutex_ref);
    }
}

/*
 * Add a client to the list
 *
 * !! The reference counter of the new client is 2:
 * one reference in the linked list "clients" plus the returned reference
 *
 * just call decr_ref to remove the last reference from "clients":
 * This will remove the client from the linked list and free it's memory
 */
struct client * add_client(int sockfd, int tunfd, int state, time_t time, struct in_addr clientIP, u_int16_t clientPort, struct in_addr vpnIP, int is_dtls_client) {
    if (n_clients >= config.max_clients) {
        if (config.debug) {
            printf("Cannot open a new connection: maximum number of connections reached\n");
        }
        return NULL;
    }

    if (config.debug) printf("Adding new client %s\n", inet_ntoa(vpnIP));
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
    conditionInit(&peer->cond_connected, NULL);
    peer->send_shutdown = 0;

    peer->is_dtls_client = is_dtls_client;
    peer->thread_running = 0;
    mutexInit(&(peer->mutex_ref), NULL);
    peer->ref_count = 2;

    createClientSSL(peer, 0);

    peer->next = clients;
    peer->prev = NULL;
    if (peer->next) peer->next->prev = peer;
    clients = peer;
    n_clients ++;
    return peer;
}

/*
 * Contains the last search results for
 * get_client_VPN and get_client_real
 */
struct client *cache_client_VPN = NULL;
struct client *cache_client_real = NULL;

/*
 * Remove a client from the list "clients"
 * Free the associated SSL structures
 */
void remove_client(struct client *peer) {
    if (config.debug) printf("Deleting the client %s\n", inet_ntoa(peer->vpnIP));

    conditionDestroy(&peer->cond_connected);
    mutexDestroy(&peer->mutex_ref);
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
    n_clients --;
}

/*
 * Get a client by its VPN IP address and increments its ref. counter
 * return NULL if the client is unknown
 */
struct client * get_client_VPN(struct in_addr *address) {
    if (cache_client_VPN != NULL
        && cache_client_VPN->vpnIP.s_addr == address->s_addr) {
        incr_ref(cache_client_VPN);
        return cache_client_VPN;
    }
    if (config.debug) printf("get %s\n", inet_ntoa(*address));
    struct client *peer = clients;
    while (peer != NULL) {
        if (peer->vpnIP.s_addr == address->s_addr) {
            /* found */
            cache_client_VPN = peer;
            incr_ref(peer);
            return peer;
        }
        peer = peer->next;
    }
    return NULL;
}

/*
 * Get a client by its real IP address and UDP port and increments its ref. counter
 * return NULL if the client is unknown
 */
struct client * get_client_real(struct sockaddr_in *cl_address) {
    if (cache_client_real != NULL
        && cache_client_real->clientaddr.sin_addr.s_addr == cl_address->sin_addr.s_addr
        && cache_client_real->clientaddr.sin_port == cl_address->sin_port) {
        incr_ref(cache_client_real);
        return cache_client_real;
    }
    if (config.debug) printf("get by client IP %s\n", inet_ntoa(cl_address->sin_addr));
    struct client *peer = clients;
    while (peer != NULL) {
        if (peer->clientaddr.sin_addr.s_addr == cl_address->sin_addr.s_addr
            && peer->clientaddr.sin_port == cl_address->sin_port) {
            /* found */
            cache_client_real = peer;
            incr_ref(peer);
            return peer;
        }
        peer = peer->next;
    }
    return NULL;
}

/*
 * Callback function used to check a certificate against a CRL
 * A transparent callback would return preverify_ok
 * If preverify_ok == 0, return.
 */
int verify_crl(int preverify_ok, X509_STORE_CTX *x509_ctx) {
    X509_REVOKED *revoked;
    int i, n;

    if (!preverify_ok)
        return preverify_ok;

    if (config.crl) {
        /* Check the name of the certificate issuer */
        if (X509_NAME_cmp(X509_CRL_get_issuer(config.crl), X509_get_issuer_name(x509_ctx->current_cert)) != 0) {
            log_message("The received certificate and the CRL have different issuers!");
            ERR_print_errors_fp(stderr);
            return 0;
        }

        /* Number of certificates in the CRL */
        n = sk_num(X509_CRL_get_REVOKED(config.crl));

        /* Compare the certificate serial number with the one of every certificates in the list */
        for (i = 0; i < n; i++) {
            revoked = (X509_REVOKED *)sk_value(X509_CRL_get_REVOKED(config.crl), i);
            if (ASN1_INTEGER_cmp(revoked->serialNumber, X509_get_serialNumber(x509_ctx->current_cert)) == 0) {
              log_message("The received certificate is revoked!");
              ERR_print_errors_fp(stderr);
              return 0;
            }
        }
    }

    return preverify_ok;
}

/*
 * Build the SSL structure for a client
 * if recreate is true, delete existing structures
 */
void createClientSSL(struct client *peer, int recreate) {
    if (recreate) {
        SSL_CTX_free(peer->ctx);
        SSL_free(peer->ssl);
    }

    SSL_METHOD *meth = peer->is_dtls_client ? DTLSv1_client_method() : DTLSv1_server_method();
    peer->ctx = SSL_CTX_new(meth);

    if (!SSL_CTX_use_certificate_chain_file(peer->ctx, config.certificate_pem)) {
        ERR_print_errors_fp(stderr);
        log_error("SSL_CTX_use_certificate_chain_file");
        log_message("%s", config.certificate_pem);
        exit(EXIT_FAILURE);
    }
    if (!SSL_CTX_use_PrivateKey_file(peer->ctx, config.key_pem, SSL_FILETYPE_PEM)) {
        ERR_print_errors_fp(stderr);
        log_error("SSL_CTX_use_PrivateKey_file");
        exit(EXIT_FAILURE);
    }
    SSL_CTX_set_verify(peer->ctx, SSL_VERIFY_PEER, verify_crl);
    SSL_CTX_set_verify_depth(peer->ctx, 1);
    if (!SSL_CTX_load_verify_locations(peer->ctx, config.verif_pem, NULL)) {
        ERR_print_errors_fp(stderr);
        log_error("SSL_CTX_load_verify_locations");
        exit(EXIT_FAILURE);
    }

    /* Mandatory for DTLS */
    SSL_CTX_set_read_ahead(peer->ctx, 1);

    peer->ssl = SSL_new(peer->ctx);
    peer->wbio = BIO_new_dgram(peer->sockfd, BIO_NOCLOSE);
    peer->rbio = BIO_new(BIO_s_fifo());
    SSL_set_bio(peer->ssl, peer->rbio, peer->wbio);

    /* No zlib compression */
    peer->ssl->ctx->comp_methods = NULL;
    /* Algorithms */
    if (strlen(config.cipher_list) != 0) {
        if (! SSL_CTX_set_cipher_list(peer->ssl->ctx, config.cipher_list)){
            log_error("SSL_CTX_set_cipher_list");
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
    }
    peer->is_dtls_client ? SSL_set_connect_state(peer->ssl) : SSL_set_accept_state(peer->ssl);

    /* Don't try to discover the MTU
     * Don't want that OpenSSL fragments our packets
     * The MTU of the TUN device is 1400 which ensure that the encrypted packets are < 1500 bytes
     * the typical overhead is 20 (IP) + 8 (UDP) + 5 (DTLS record) + 20 (160 bits SHA1 MAC)
     */
    SSL_set_options(peer->ssl, SSL_OP_NO_QUERY_MTU);
    peer->ssl->d1->mtu = 1500;
}

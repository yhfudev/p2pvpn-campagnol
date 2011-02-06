/*
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
 * DTLS contexts and SSL structure management
 */

#include "campagnol.h"
#include "dtls_utils.h"
#include "../common/log.h"
#include "../common/bss_fifo.h"
#include "bf_rate_limiter.h"
#include "communication.h"
#include "../common/pthread_wrap.h"

/* SSL contexts */
static SSL_CTX *campagnol_ctx_client;
static SSL_CTX *campagnol_ctx_server;
static pthread_mutex_t ctx_lock;

/* mutexes for OpenSSL internal use */
static pthread_mutex_t *crypto_mutexes = NULL;

/*
 * Callback function for certificate validation
 * A transparent callback would return ok
 * Log error messages and accept an old CRL.
 */
static int verify_callback(int ok, X509_STORE_CTX *ctx) {
    const char *buf;
    X509 *err_cert;
    int err, depth;
    err_cert = X509_STORE_CTX_get_current_cert(ctx);
    err = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);
    BIO *bio_buf = NULL;

    if (!ok) {
        bio_buf = BIO_new(BIO_s_mem());
        if (bio_buf) {
            X509_NAME_print_ex(bio_buf, X509_get_subject_name(err_cert), 0,
                    XN_FLAG_ONELINE);
            BIO_write(bio_buf, "", 1);
            BIO_get_mem_data(bio_buf, &buf);
        }
        else {
            buf = "";
        }

        log_message("Error while verifying a certificate at depth=%d",
                depth);
        log_message("Subject: %s", buf);
        log_message("Verify error (%d): %s", err,
                X509_verify_cert_error_string(err));
        if (depth > config.verify_depth) {
            log_message("%s", X509_verify_cert_error_string(
                            X509_V_ERR_CERT_CHAIN_TOO_LONG));
        }

        BIO_free(bio_buf);

        switch (ctx->error) {
            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
                bio_buf = BIO_new(BIO_s_mem());
                if (bio_buf) {
                    X509_NAME_print_ex(bio_buf, X509_get_issuer_name(
                            ctx->current_cert), 0, XN_FLAG_ONELINE);
                    BIO_write(bio_buf, "", 1);
                    BIO_get_mem_data(bio_buf, &buf);
                }
                else {
                    buf = "";
                }
                log_message("Issuer: %s", buf);
                BIO_free(bio_buf);
                break;
            case X509_V_ERR_CRL_HAS_EXPIRED:
                log_message("Warning: the CRL has expired. Ignoring.");
                ok = 1;
                break;
        }
    }
    return (ok);
}

/*
 * callback function for SSL_*_set_info_callback
 * The callback is called whenever the state of the TLS connection changes.
 */
static void ctx_info_callback(const SSL *ssl, int where, int ret) {
    if (where & SSL_CB_ALERT) {
        log_message("DTLS alert: %s: %s | %s",
                SSL_alert_type_string_long(ret),
                SSL_alert_desc_string_long(ret),
                SSL_state_string_long(ssl));
    }
}

/*
 * Allocate and configure a DTLS context
 */
static SSL_CTX * createContext(int is_client) {
    SSL_CTX *ctx;

    if (is_client) {
        ctx = SSL_CTX_new(DTLSv1_client_method());
    }
    else {
        ctx = SSL_CTX_new(DTLSv1_server_method());
    }
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        log_error(-1, "SSL_CTX_new");
        return NULL;
    }
    if (!SSL_CTX_use_certificate_chain_file(ctx, config.certificate_pem)) {
        ERR_print_errors_fp(stderr);
        log_error(-1, "SSL_CTX_use_certificate_chain_file (%s)",
                config.certificate_pem);
        SSL_CTX_free(ctx);
        return NULL;
    }
    if (!SSL_CTX_use_PrivateKey_file(ctx, config.key_pem, SSL_FILETYPE_PEM)) {
        ERR_print_errors_fp(stderr);
        log_error(-1, "SSL_CTX_use_PrivateKey_file");
        SSL_CTX_free(ctx);
        return NULL;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
            verify_callback);

    if (config.verify_depth > 0) {
        SSL_CTX_set_verify_depth(ctx, config.verify_depth);
    }
    if (!SSL_CTX_load_verify_locations(ctx, config.verif_pem, config.verif_dir)) {
        ERR_print_errors_fp(stderr);
        log_error(-1, "SSL_CTX_load_verify_locations");
        SSL_CTX_free(ctx);
        return NULL;
    }

    // add CRL
    if (config.crl) {
        X509_STORE *x509 = SSL_CTX_get_cert_store(ctx);
        X509_LOOKUP *file_lookup = X509_STORE_add_lookup(x509,
                X509_LOOKUP_file());
        X509_load_crl_file(file_lookup, config.crl, X509_FILETYPE_PEM);
        X509_STORE_set_flags(x509, X509_V_FLAG_CRL_CHECK);
    }

    // validate the key
    if (!SSL_CTX_check_private_key(ctx)) {
        log_error(-1, "SSL_CTX_check_private_key");
        SSL_CTX_free(ctx);
        return NULL;
    }

    if (config.debug) {
        SSL_CTX_set_info_callback(ctx, ctx_info_callback);
    }

    /* Mandatory for DTLS */
    SSL_CTX_set_read_ahead(ctx, 1);

    /* No zlib compression */
#ifdef SSL_OP_NO_COMPRESSION
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
#else
    ctx->comp_methods = NULL;
#endif
    /* Algorithms */
    if (config.cipher_list != NULL) {
        if (!SSL_CTX_set_cipher_list(ctx, config.cipher_list)) {
            log_error(-1, "SSL_CTX_set_cipher_list");
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            return NULL;
        }
    }

    if (!is_client) {
        SSL_CTX_set_client_CA_list(ctx, SSL_load_client_CA_file(
                config.verif_pem));
    }

    return ctx;
}

/*
 * Create two DTLS context: client and server
 * return -1 on error.
 */
int initDTLS() {
    campagnol_ctx_client = createContext(1);
    if (campagnol_ctx_client == NULL) {
        log_error(-1, "Cannot allocate a new SSL context");
        return -1;
    }
    campagnol_ctx_server = createContext(0);
    if (campagnol_ctx_server == NULL) {
        SSL_CTX_free(campagnol_ctx_client);
        log_error(-1, "Cannot allocate a new SSL context");
        return -1;
    }
    mutexInit(&ctx_lock, NULL);
    return 0;
}

void clearDTLS() {
    SSL_CTX_free(campagnol_ctx_client);
    SSL_CTX_free(campagnol_ctx_server);
    mutexDestroy(&ctx_lock);
}

int rebuildDTLS() {
    SSL_CTX *tmp;
    mutexLock(&ctx_lock);
    tmp = createContext(1);
    if (tmp != NULL) {
        SSL_CTX_free(campagnol_ctx_client);
        campagnol_ctx_client = tmp;
    }
    else {
        log_error(-1, "Cannot allocate a new SSL context");
        mutexUnlock(&ctx_lock);
        return -1;
    }

    tmp = createContext(0);
    if (tmp != NULL) {
        SSL_CTX_free(campagnol_ctx_server);
        campagnol_ctx_server = tmp;
    }
    else {
        log_error(-1, "Cannot allocate a new SSL context");
        mutexUnlock(&ctx_lock);
        return -1;
    }

    mutexUnlock(&ctx_lock);
    return 0;
}

/*
 * Build the SSL structure for a client
 */
int createClientSSL(struct client *peer) {
    struct timeval recv_timeout;
    BIO *wbio_tmp;

    mutexLock(&ctx_lock);

    if (peer->is_dtls_client) {
        peer->ctx = campagnol_ctx_client;
    }
    else {
        peer->ctx = campagnol_ctx_server;
    }

    peer->ssl = SSL_new(peer->ctx);
    if (peer->ssl == NULL) {
        ERR_print_errors_fp(stderr);
        log_error(-1, "SSL_new");
        mutexUnlock(&ctx_lock);
        return -1;
    }
    wbio_tmp = BIO_new_dgram(peer->sockfd, BIO_NOCLOSE);
    if (wbio_tmp == NULL) {
        ERR_print_errors_fp(stderr);
        log_error(-1, "BIO_new_dgram");
        SSL_free(peer->ssl);
        mutexUnlock(&ctx_lock);
        return -1;
    }
    /* create a BIO for the rate limiter if required */
    if (config.tb_client_size != 0 || config.tb_connection_size != 0) {
        struct tb_state *global =
                (config.tb_client_size != 0) ? &global_rate_limiter : NULL;
        struct tb_state *local =
                (config.tb_connection_size != 0) ? &peer->rate_limiter : NULL;
        peer->wbio = BIO_f_new_rate_limiter(global, local);
        if (peer->wbio == NULL) {
            ERR_print_errors_fp(stderr);
            log_error(-1, "BIO_f_new_rate_limiter");
            BIO_free(wbio_tmp);
            SSL_free(peer->ssl);
            mutexUnlock(&ctx_lock);
            return -1;
        }
        BIO_push(peer->wbio, wbio_tmp);
    }
    else {
        peer->wbio = wbio_tmp;
    }

    peer->rbio = BIO_new_fifo(config.FIFO_size, MESSAGE_MAX_LENGTH);
    if (peer->rbio == NULL) {
        ERR_print_errors_fp(stderr);
        log_error(-1, "BIO_new_fifo");
        BIO_free_all(peer->wbio);
        SSL_free(peer->ssl);
        mutexUnlock(&ctx_lock);
        return -1;
    }
    recv_timeout.tv_usec = PEER_RECV_TIMEMOUT_USEC;
    recv_timeout.tv_sec = PEER_RECV_TIMEOUT_SEC;
    BIO_ctrl(peer->rbio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &recv_timeout);
    SSL_set_bio(peer->ssl, peer->rbio, peer->wbio);

    peer->out_fifo = BIO_new_fifo(config.FIFO_size, MESSAGE_MAX_LENGTH);
    if (peer->out_fifo == NULL) {
        ERR_print_errors_fp(stderr);
        log_error(-1, "BIO_new_fifo");
        SSL_free(peer->ssl);
        mutexUnlock(&ctx_lock);
        return -1;
    }
    /* The FIFO for the outgoing SSL stream is created with a drop tail policy
     * until the DTLS session is opened and we start transmitting VPN packets
     */
    BIO_ctrl(peer->out_fifo, BIO_CTRL_FIFO_SET_DROPTAIL, 1, NULL);

    if (peer->is_dtls_client) {
        SSL_set_connect_state(peer->ssl);
    }
    else {
        SSL_set_accept_state(peer->ssl);
    }

    /* Don't try to discover the MTU
     */
    SSL_set_options(peer->ssl, SSL_OP_NO_QUERY_MTU);
    peer->ssl->d1->mtu = config.tun_mtu;

    mutexUnlock(&ctx_lock);
    return 0;
}

/* locking callback function for openssl */
static void openssl_locking_function(int mode, int n, const char *file __attribute__((unused)),
        int line __attribute__((unused))) {
    if (mode & CRYPTO_LOCK) {
        mutexLock(&crypto_mutexes[n]);
    }
    else {
        mutexUnlock(&crypto_mutexes[n]);
    }
}

#ifndef HAVE_CRYPTO_THREADID_CURRENT
/* id_function for openssl threading support
 * openssl >= 1.0.0 has a default implementation */
static unsigned long openssl_id_function(void) {
    /* linux: pthread_t is a unsigned long int, ok
     * freebsd/openbsd: pthread_t is a pointer, ok
     */
    return (unsigned long) pthread_self();
}
#endif

/* setup callback functions for openssl threading support */
void setup_openssl_thread() {
    int i, n;
    n = CRYPTO_num_locks();

    crypto_mutexes = CHECK_ALLOC_FATAL(malloc(sizeof(pthread_mutex_t)*n));
    for (i = 0; i < n; i++) {
        mutexInit(&crypto_mutexes[i], NULL);
    }
#ifndef HAVE_CRYPTO_THREADID_CURRENT
    CRYPTO_set_id_callback(&openssl_id_function);
#endif
    CRYPTO_set_locking_callback(&openssl_locking_function);
}

/* clean callback functions */
void cleanup_openssl_thread() {
    int i, n;
    n = CRYPTO_num_locks();

#ifndef HAVE_CRYPTO_THREADID_CURRENT
    CRYPTO_set_id_callback(NULL);
#endif
    CRYPTO_set_locking_callback(NULL);

    for (i = 0; i < n; i++) {
        mutexDestroy(&crypto_mutexes[i]);
    }
    free(crypto_mutexes);
}

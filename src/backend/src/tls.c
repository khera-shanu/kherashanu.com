#include "tls.h"
#include <openssl/err.h>
#include <stdio.h>

static SSL_CTX *ssl_ctx = NULL;

int tls_init(const char *cert_path, const char *key_path) {
  // Initialize OpenSSL
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();

  // Create context with TLS server method
  ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!ssl_ctx) {
    fprintf(stderr, "TLS: Failed to create SSL context\n");
    ERR_print_errors_fp(stderr);
    return -1;
  }

  // Set minimum TLS version to 1.2
  SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);

  // Load certificate
  if (SSL_CTX_use_certificate_file(ssl_ctx, cert_path, SSL_FILETYPE_PEM) <= 0) {
    fprintf(stderr, "TLS: Failed to load certificate: %s\n", cert_path);
    ERR_print_errors_fp(stderr);
    return -1;
  }

  // Load private key
  if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
    fprintf(stderr, "TLS: Failed to load private key: %s\n", key_path);
    ERR_print_errors_fp(stderr);
    return -1;
  }

  // Verify private key matches certificate
  if (!SSL_CTX_check_private_key(ssl_ctx)) {
    fprintf(stderr, "TLS: Private key does not match certificate\n");
    return -1;
  }

  printf("TLS: Initialized with cert=%s key=%s\n", cert_path, key_path);
  return 0;
}

SSL_CTX *tls_get_context(void) { return ssl_ctx; }

SSL *tls_wrap_socket(int fd) {
  if (!ssl_ctx)
    return NULL;

  SSL *ssl = SSL_new(ssl_ctx);
  if (!ssl) {
    ERR_print_errors_fp(stderr);
    return NULL;
  }

  SSL_set_fd(ssl, fd);
  return ssl;
}

int tls_accept(SSL *ssl) {
  int ret = SSL_accept(ssl);
  if (ret <= 0) {
    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      return 0; // Would block, try again
    }
    ERR_print_errors_fp(stderr);
    return -1;
  }
  return 1; // Success
}

int tls_read(SSL *ssl, void *buf, size_t len) {
  int ret = SSL_read(ssl, buf, (int)len);
  if (ret <= 0) {
    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      return 0; // Would block
    }
    if (err == SSL_ERROR_ZERO_RETURN) {
      return 0; // Connection closed
    }
    return -1; // Error
  }
  return ret;
}

int tls_write(SSL *ssl, const void *buf, size_t len) {
  int ret = SSL_write(ssl, buf, (int)len);
  if (ret <= 0) {
    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      return 0; // Would block
    }
    return -1; // Error
  }
  return ret;
}

void tls_close(SSL *ssl) {
  if (ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }
}

void tls_cleanup(void) {
  if (ssl_ctx) {
    SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
  }
  EVP_cleanup();
}

#ifndef TLS_H
#define TLS_H

#include <openssl/ssl.h>
#include <stdbool.h>

// Initialize TLS context with certificate and key
int tls_init(const char *cert_path, const char *key_path);

// Get the SSL context
SSL_CTX *tls_get_context(void);

// Create SSL connection for socket
SSL *tls_wrap_socket(int fd);

// Perform SSL accept (handshake)
int tls_accept(SSL *ssl);

// Read from SSL connection
int tls_read(SSL *ssl, void *buf, size_t len);

// Write to SSL connection
int tls_write(SSL *ssl, const void *buf, size_t len);

// Close SSL connection
void tls_close(SSL *ssl);

// Cleanup TLS context
void tls_cleanup(void);

#endif // TLS_H

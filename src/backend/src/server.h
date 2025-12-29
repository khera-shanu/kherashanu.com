#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define MAX_CONNECTIONS 1024
#define READ_BUFFER_SIZE                                                       \
  (8 * 1024 * 1024) /* 8MB - enough for large blog posts */
#define WEBROOT_PATH_MAX 512

typedef struct {
  int http_port;
  int https_port;
  char cert_path[256];
  char key_path[256];
  char webroot[WEBROOT_PATH_MAX];
  bool use_tls;
} server_config_t;

typedef enum {
  CONN_STATE_NONE,
  CONN_STATE_READING_REQUEST,
  CONN_STATE_WRITING_RESPONSE,
  CONN_STATE_CLOSING
} connection_state_t;

typedef struct {
  int fd;
  SSL *ssl;
  bool is_tls;

  // Request reading
  char read_buffer[READ_BUFFER_SIZE];
  size_t read_pos;

  // Response writing
  char *write_buffer;
  size_t write_len;
  size_t write_pos;

  connection_state_t state;
  time_t last_activity;
} connection_t;

// Initialize server with configuration
int server_init(server_config_t *config);

// Main server loop
int server_run(void);

// Cleanup
void server_shutdown(void);

// Get webroot path
const char *server_get_webroot(void);

#endif // SERVER_H

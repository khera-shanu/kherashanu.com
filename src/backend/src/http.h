#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>

#define HTTP_METHOD_MAX 16
#define HTTP_PATH_MAX 2048
#define HTTP_VERSION_MAX 16
#define HTTP_HEADER_MAX 8192
#define HTTP_MAX_HEADERS 32
#define HTTP_MAX_BODY_SIZE (10 * 1024 * 1024) /* 10 MB */

/* HTTP header */
typedef struct {
  char *name;
  char *value;
} http_header_t;

typedef struct {
  char method[HTTP_METHOD_MAX];
  char path[HTTP_PATH_MAX];
  char version[HTTP_VERSION_MAX];
  bool keep_alive;

  /* Parsed headers */
  http_header_t headers[HTTP_MAX_HEADERS];
  int header_count;

  /* Body info (body data is separate) */
  size_t content_length;
  const char *content_type;
} http_request_t;

typedef struct {
  int status_code;
  const char *status_text;
  const char *content_type;
  const char *cache_control;
  const char *content_encoding;
  const char *body;
  size_t body_len;
  bool keep_alive;
} http_response_t;

// Compress data using GZIP
// Returns newly allocated buffer (must be freed) or NULL on error
// Updates out_len with compressed size
char *http_compress_gzip(const char *data, size_t len, size_t *out_len);

// Parse HTTP request from buffer
// Returns 0 on success, -1 on error, 1 if incomplete
int http_parse_request(const char *buffer, size_t len, http_request_t *req);

// Clean up request (free allocated headers)
void http_request_cleanup(http_request_t *req);

// Build HTTP response into buffer
// Returns number of bytes written, or -1 on error
int http_build_response(const http_response_t *resp, char *buffer,
                        size_t buffer_size);

// Get status text for code
const char *http_status_text(int code);

// URL decode path (handles %XX encoding)
void http_url_decode(const char *src, char *dst, size_t dst_size);

// Get header value by name (case-insensitive)
const char *http_get_header(const http_request_t *req, const char *name);

#endif // HTTP_H

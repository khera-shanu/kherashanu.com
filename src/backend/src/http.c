#include "http.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

const char *http_status_text(int code) {
  switch (code) {
  case 200:
    return "OK";
  case 201:
    return "Created";
  case 206:
    return "Partial Content";
  case 301:
    return "Moved Permanently";
  case 304:
    return "Not Modified";
  case 400:
    return "Bad Request";
  case 401:
    return "Unauthorized";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 500:
    return "Internal Server Error";
  case 501:
    return "Not Implemented";
  default:
    return "Unknown";
  }
}

int http_parse_request(const char *buffer, size_t len, http_request_t *req) {
  (void)len; /* May use for validation later */

  /* Initialize request */
  memset(req, 0, sizeof(http_request_t));

  // Check for complete request (ends with \r\n\r\n)
  const char *end = strstr(buffer, "\r\n\r\n");
  if (!end) {
    return 1; // Incomplete
  }

  // Parse request line: METHOD PATH VERSION\r\n
  const char *line_end = strstr(buffer, "\r\n");
  if (!line_end) {
    return -1;
  }

  // Extract method
  const char *p = buffer;
  size_t i = 0;
  while (p < line_end && *p != ' ' && i < HTTP_METHOD_MAX - 1) {
    req->method[i++] = *p++;
  }
  req->method[i] = '\0';

  if (*p != ' ')
    return -1;
  p++; // Skip space

  // Extract full path (including query string for framework)
  i = 0;
  while (p < line_end && *p != ' ' && i < HTTP_PATH_MAX - 1) {
    req->path[i++] = *p++;
  }
  req->path[i] = '\0';

  if (*p != ' ')
    return -1;
  p++; // Skip space

  // Extract version
  i = 0;
  while (p < line_end && i < HTTP_VERSION_MAX - 1) {
    req->version[i++] = *p++;
  }
  req->version[i] = '\0';

  // Default to keep-alive for HTTP/1.1
  req->keep_alive = (strstr(req->version, "1.1") != NULL);

  // Parse headers
  p = line_end + 2; // Skip \r\n
  while (p < end && req->header_count < HTTP_MAX_HEADERS) {
    const char *header_end = strstr(p, "\r\n");
    if (!header_end || header_end == p)
      break;

    // Find colon
    const char *colon = strchr(p, ':');
    if (!colon || colon > header_end) {
      p = header_end + 2;
      continue;
    }

    // Extract header name
    size_t name_len = colon - p;
    char *name = malloc(name_len + 1);
    if (!name)
      break;
    memcpy(name, p, name_len);
    name[name_len] = '\0';

    // Skip colon and whitespace
    colon++;
    while (colon < header_end && (*colon == ' ' || *colon == '\t'))
      colon++;

    // Extract header value
    size_t value_len = header_end - colon;
    char *value = malloc(value_len + 1);
    if (!value) {
      free(name);
      break;
    }
    memcpy(value, colon, value_len);
    value[value_len] = '\0';

    // Store header
    req->headers[req->header_count].name = name;
    req->headers[req->header_count].value = value;
    req->header_count++;

    // Check for special headers
    if (strcasecmp(name, "Content-Length") == 0) {
      char *endptr;
      unsigned long long cls = strtoull(value, &endptr, 10);
      /* Valid integers only, no extra garbage, no negative sign */
      if (endptr == value || *endptr != '\0' || strchr(value, '-')) {
        req->content_length = 0;
      } else {
        req->content_length = (size_t)cls;
      }
    } else if (strcasecmp(name, "Content-Type") == 0) {
      req->content_type = value; /* Points to allocated string */
    } else if (strcasecmp(name, "Connection") == 0) {
      if (strcasecmp(value, "close") == 0) {
        req->keep_alive = false;
      } else if (strcasecmp(value, "keep-alive") == 0) {
        req->keep_alive = true;
      }
    }

    p = header_end + 2;
  }

  return 0;
}

void http_request_cleanup(http_request_t *req) {
  if (!req)
    return;

  for (int i = 0; i < req->header_count; i++) {
    free(req->headers[i].name);
    free(req->headers[i].value);
  }
  req->header_count = 0;
}

const char *http_get_header(const http_request_t *req, const char *name) {
  if (!req || !name)
    return NULL;

  for (int i = 0; i < req->header_count; i++) {
    if (strcasecmp(req->headers[i].name, name) == 0) {
      return req->headers[i].value;
    }
  }
  return NULL;
}

#include <zlib.h>

char *http_compress_gzip(const char *data, size_t len, size_t *out_len) {
  if (!data || len == 0)
    return NULL;

  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    return NULL;
  }

  size_t bound = deflateBound(&strm, len);
  char *out = malloc(bound);
  if (!out) {
    deflateEnd(&strm);
    return NULL;
  }

  strm.avail_in = len;
  strm.next_in = (Bytef *)data;
  strm.avail_out = bound;
  strm.next_out = (Bytef *)out;

  if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
    free(out);
    deflateEnd(&strm);
    return NULL;
  }

  *out_len = bound - strm.avail_out;
  deflateEnd(&strm);
  return out;
}

int http_build_response(const http_response_t *resp, char *buffer,
                        size_t buffer_size) {
  char cache_control[256] = "";
  if (resp->cache_control) {
    snprintf(cache_control, sizeof(cache_control), "Cache-Control: %s\r\n",
             resp->cache_control);
  }

  char content_encoding[64] = "";
  if (resp->content_encoding) {
    snprintf(content_encoding, sizeof(content_encoding),
             "Content-Encoding: %s\r\n", resp->content_encoding);
  }

  int header_len =
      snprintf(buffer, buffer_size,
               "HTTP/1.1 %d %s\r\n"
               "Server: Kherashanu/1.0\r\n"
               "Content-Type: %s\r\n"
               "Content-Length: %zu\r\n"
               "Connection: %s\r\n"
               "%s" // Cache-Control
               "%s" // Content-Encoding
               "\r\n",
               resp->status_code,
               resp->status_text ? resp->status_text
                                 : http_status_text(resp->status_code),
               resp->content_type, resp->body_len,
               resp->keep_alive ? "keep-alive" : "close", cache_control,
               content_encoding);

  if (header_len < 0 || (size_t)header_len >= buffer_size) {
    return -1;
  }

  return header_len;
}

void http_url_decode(const char *src, char *dst, size_t dst_size) {
  size_t i = 0;
  while (*src && i < dst_size - 1) {
    if (*src == '%' && isxdigit((unsigned char)src[1]) &&
        isxdigit((unsigned char)src[2])) {
      char hex[3] = {src[1], src[2], '\0'};
      dst[i++] = (char)strtol(hex, NULL, 16);
      src += 3;
    } else if (*src == '+') {
      dst[i++] = ' ';
      src++;
    } else {
      dst[i++] = *src++;
    }
  }
  dst[i] = '\0';
}

/**
 * Kherashanu Web Framework - Flask-like routing and request handling
 */
#ifndef KFW_FRAMEWORK_H
#define KFW_FRAMEWORK_H

#include "../http.h"
#include "../json/json.h"
#include <stdbool.h>
#include <stddef.h>

/* Error codes */
typedef enum {
  KFW_OK = 0,
  KFW_ERR_NOMEM,
  KFW_ERR_NOT_FOUND,
  KFW_ERR_METHOD_NOT_ALLOWED,
  KFW_ERR_BAD_REQUEST,
  KFW_ERR_UNAUTHORIZED,
  KFW_ERR_FORBIDDEN,
  KFW_ERR_INTERNAL
} kfw_error_t;

/* Forward declarations */
typedef struct kfw_app kfw_app_t;
typedef struct kfw_request kfw_request_t;
typedef struct kfw_response kfw_response_t;

/* Maximum limits */
#define KFW_MAX_ROUTES 64
#define KFW_MAX_PARAMS 16
#define KFW_MAX_HEADERS 32
#define KFW_MAX_PATH 2048
#define KFW_MAX_METHOD 16

/* Request context */
struct kfw_request {
  /* HTTP method/verb */
  char method[KFW_MAX_METHOD];

  /* Full path (e.g. /api/blog/hello-world) */
  char path[KFW_MAX_PATH];

  /* Path parameters extracted from route pattern */
  struct {
    char *key;
    char *value;
  } path_params[KFW_MAX_PARAMS];
  int path_params_count;

  /* Query string parameters */
  struct {
    char *key;
    char *value;
  } query_params[KFW_MAX_PARAMS];
  int query_params_count;

  /* Request headers */
  struct {
    char *name;
    char *value;
  } headers[KFW_MAX_HEADERS];
  int header_count;

  /* Request body */
  const char *body;
  size_t body_len;

  /* Parsed JSON body (if Content-Type is application/json) */
  json_value_t *json;

  /* Client info */
  char client_ip[64];
};

/* Response builder */
struct kfw_response {
  int status_code;
  char *content_type;
  char *body;
  size_t body_len;
  bool body_allocated; /* true if body should be freed */

  /* Response headers */
  struct {
    char *name;
    char *value;
  } headers[KFW_MAX_HEADERS];
  int header_count;
};

/* Handler function signature */
typedef kfw_error_t (*kfw_handler_fn)(kfw_request_t *req, kfw_response_t *resp);

/* Middleware function signature */
typedef kfw_error_t (*kfw_middleware_fn)(kfw_request_t *req,
                                         kfw_response_t *resp,
                                         kfw_handler_fn next);

/* Route definition */
typedef struct {
  char method[KFW_MAX_METHOD];
  char pattern[KFW_MAX_PATH];
  kfw_handler_fn handler;
} kfw_route_t;

/* Application structure */
struct kfw_app {
  kfw_route_t routes[KFW_MAX_ROUTES];
  int route_count;

  /* Global error handler */
  kfw_handler_fn error_handler;

  /* User data pointer */
  void *user_data;
};

/* ========== App lifecycle ========== */

/**
 * Create a new application instance
 */
kfw_app_t *kfw_app_create(void);

/**
 * Destroy application and free resources
 */
void kfw_app_destroy(kfw_app_t *app);

/**
 * Set user data pointer
 */
void kfw_app_set_userdata(kfw_app_t *app, void *data);

/**
 * Get user data pointer
 */
void *kfw_app_get_userdata(kfw_app_t *app);

/* ========== Route registration ========== */

/**
 * Register a route handler
 * Pattern can include :param placeholders (e.g. /api/blog/:slug)
 */
bool kfw_route(kfw_app_t *app, const char *method, const char *pattern,
               kfw_handler_fn handler);

/* Convenience macros */
#define KFW_GET(app, pattern, handler) kfw_route(app, "GET", pattern, handler)
#define KFW_POST(app, pattern, handler) kfw_route(app, "POST", pattern, handler)
#define KFW_PUT(app, pattern, handler) kfw_route(app, "PUT", pattern, handler)
#define KFW_DELETE(app, pattern, handler)                                      \
  kfw_route(app, "DELETE", pattern, handler)
#define KFW_ROUTE(app, pattern, method, handler)                               \
  kfw_route(app, method, pattern, handler)

/* ========== Request handling ========== */

/**
 * Handle an incoming HTTP request
 * Parses the request, finds matching route, executes handler
 *
 * @param app       Application instance
 * @param http_req  Parsed HTTP request
 * @param body      Request body (can be NULL)
 * @param body_len  Body length
 * @param http_resp Output HTTP response
 * @return KFW_OK on success
 */
kfw_error_t kfw_handle(kfw_app_t *app, const http_request_t *http_req,
                       const char *body, size_t body_len,
                       http_response_t *http_resp);

/* ========== Request accessors ========== */

/**
 * Get path parameter by name
 * @return Parameter value or NULL if not found
 */
const char *kfw_req_param(kfw_request_t *req, const char *name);

/**
 * Get query parameter by name
 * @return Parameter value or NULL if not found
 */
const char *kfw_req_query(kfw_request_t *req, const char *name);

/**
 * Get header by name (case-insensitive)
 */
const char *kfw_req_header(kfw_request_t *req, const char *name);

/**
 * Get JSON value from request body by path (e.g. "user.name")
 */
json_value_t *kfw_req_json_get(kfw_request_t *req, const char *path);

/* ========== Response builders ========== */

/**
 * Send JSON response
 */
void kfw_resp_json(kfw_response_t *resp, json_value_t *json);

/**
 * Send text response
 */
void kfw_resp_text(kfw_response_t *resp, int status, const char *text);

/**
 * Send error response as JSON: {"error": "message"}
 */
void kfw_resp_error(kfw_response_t *resp, int status, const char *message);

/**
 * Set response header
 */
bool kfw_resp_header(kfw_response_t *resp, const char *name, const char *value);

/**
 * Set response status code
 */
void kfw_resp_status(kfw_response_t *resp, int status);

/* ========== Utilities ========== */

/**
 * Parse query string into key-value pairs
 */
int kfw_parse_query_string(const char *query, char **keys, char **values,
                           int max_pairs);

/**
 * URL decode a string
 */
void kfw_url_decode(const char *src, char *dst, size_t dst_size);

/**
 * Match route pattern against path
 * Extracts path parameters if matched
 * @return true if matched
 */
bool kfw_match_route(const char *pattern, const char *path, kfw_request_t *req);

/* ========== Request/Response cleanup ========== */

/**
 * Initialize request structure
 */
void kfw_request_init(kfw_request_t *req);

/**
 * Clean up request (free allocated memory)
 */
void kfw_request_cleanup(kfw_request_t *req);

/**
 * Initialize response structure
 */
void kfw_response_init(kfw_response_t *resp);

/**
 * Clean up response (free allocated memory)
 */
void kfw_response_cleanup(kfw_response_t *resp);

#endif /* KFW_FRAMEWORK_H */

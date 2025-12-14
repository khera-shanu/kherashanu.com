/**
 * Kherashanu Web Framework - Implementation
 */
#include "framework.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ========== App lifecycle ========== */

kfw_app_t *kfw_app_create(void) {
  kfw_app_t *app = calloc(1, sizeof(kfw_app_t));
  return app;
}

void kfw_app_destroy(kfw_app_t *app) {
  if (app) {
    free(app);
  }
}

void kfw_app_set_userdata(kfw_app_t *app, void *data) {
  if (app)
    app->user_data = data;
}

void *kfw_app_get_userdata(kfw_app_t *app) {
  return app ? app->user_data : NULL;
}

/* ========== Route registration ========== */

bool kfw_route(kfw_app_t *app, const char *method, const char *pattern,
               kfw_handler_fn handler) {
  if (!app || !method || !pattern || !handler)
    return false;

  if (app->route_count >= KFW_MAX_ROUTES)
    return false;

  kfw_route_t *route = &app->routes[app->route_count];
  strncpy(route->method, method, KFW_MAX_METHOD - 1);
  route->method[KFW_MAX_METHOD - 1] = '\0';
  strncpy(route->pattern, pattern, KFW_MAX_PATH - 1);
  route->pattern[KFW_MAX_PATH - 1] = '\0';
  route->handler = handler;

  app->route_count++;
  return true;
}

/* ========== URL decoding ========== */

void kfw_url_decode(const char *src, char *dst, size_t dst_size) {
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

/* ========== Query string parsing ========== */

int kfw_parse_query_string(const char *query, char **keys, char **values,
                           int max_pairs) {
  if (!query || !keys || !values)
    return 0;

  int count = 0;
  const char *p = query;

  while (*p && count < max_pairs) {
    /* Find key */
    const char *key_start = p;
    while (*p && *p != '=' && *p != '&')
      p++;

    if (p == key_start)
      break;

    size_t key_len = p - key_start;
    char *key = malloc(key_len + 1);
    if (!key)
      break;
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';

    /* URL decode key */
    char decoded_key[256];
    kfw_url_decode(key, decoded_key, sizeof(decoded_key));
    free(key);
    key = strdup(decoded_key);
    if (!key)
      break;

    char *value = NULL;
    if (*p == '=') {
      p++; /* Skip = */
      const char *val_start = p;
      while (*p && *p != '&')
        p++;

      size_t val_len = p - val_start;
      char *raw_value = malloc(val_len + 1);
      if (!raw_value) {
        free(key);
        break;
      }
      memcpy(raw_value, val_start, val_len);
      raw_value[val_len] = '\0';

      /* URL decode value */
      char decoded_val[1024];
      kfw_url_decode(raw_value, decoded_val, sizeof(decoded_val));
      free(raw_value);
      value = strdup(decoded_val);
    } else {
      value = strdup("");
    }

    if (!value) {
      free(key);
      break;
    }

    keys[count] = key;
    values[count] = value;
    count++;

    if (*p == '&')
      p++;
  }

  return count;
}

/* ========== Route matching ========== */

bool kfw_match_route(const char *pattern, const char *path,
                     kfw_request_t *req) {
  const char *p = pattern;
  const char *s = path;

  while (*p && *s) {
    if (*p == ':') {
      /* Path parameter */
      p++; /* Skip : */

      /* Extract parameter name */
      const char *param_start = p;
      while (*p && *p != '/')
        p++;
      size_t param_len = p - param_start;

      /* Extract parameter value from path */
      const char *val_start = s;
      while (*s && *s != '/')
        s++;
      size_t val_len = s - val_start;

      if (req && req->path_params_count < KFW_MAX_PARAMS) {
        /* Store parameter */
        char *key = malloc(param_len + 1);
        char *val = malloc(val_len + 1);
        if (key && val) {
          memcpy(key, param_start, param_len);
          key[param_len] = '\0';
          memcpy(val, val_start, val_len);
          val[val_len] = '\0';

          /* URL decode value */
          char decoded[256];
          kfw_url_decode(val, decoded, sizeof(decoded));
          free(val);
          val = strdup(decoded);

          req->path_params[req->path_params_count].key = key;
          req->path_params[req->path_params_count].value = val;
          req->path_params_count++;
        } else {
          free(key);
          free(val);
        }
      }
    } else if (*p == *s) {
      p++;
      s++;
    } else {
      return false;
    }
  }

  /* Both should be at end, or pattern ended with param that consumed rest */
  return (*p == '\0' && *s == '\0');
}

/* ========== Request accessors ========== */

const char *kfw_req_param(kfw_request_t *req, const char *name) {
  if (!req || !name)
    return NULL;

  for (int i = 0; i < req->path_params_count; i++) {
    if (strcmp(req->path_params[i].key, name) == 0) {
      return req->path_params[i].value;
    }
  }
  return NULL;
}

const char *kfw_req_query(kfw_request_t *req, const char *name) {
  if (!req || !name)
    return NULL;

  for (int i = 0; i < req->query_params_count; i++) {
    if (strcmp(req->query_params[i].key, name) == 0) {
      return req->query_params[i].value;
    }
  }
  return NULL;
}

const char *kfw_req_header(kfw_request_t *req, const char *name) {
  if (!req || !name)
    return NULL;

  for (int i = 0; i < req->header_count; i++) {
    if (strcasecmp(req->headers[i].name, name) == 0) {
      return req->headers[i].value;
    }
  }
  return NULL;
}

json_value_t *kfw_req_json_get(kfw_request_t *req, const char *path) {
  if (!req || !req->json || !path)
    return NULL;

  json_value_t *current = req->json;
  char *path_copy = strdup(path);
  if (!path_copy)
    return NULL;

  char *saveptr;
  char *token = strtok_r(path_copy, ".", &saveptr);

  while (token && current) {
    if (json_is_object(current)) {
      current = json_object_get(current, token);
    } else if (json_is_array(current)) {
      char *endptr;
      long idx = strtol(token, &endptr, 10);
      if (*endptr == '\0' && idx >= 0) {
        current = json_array_get(current, (size_t)idx);
      } else {
        current = NULL;
      }
    } else {
      current = NULL;
    }
    token = strtok_r(NULL, ".", &saveptr);
  }

  free(path_copy);
  return current;
}

/* ========== Response builders ========== */

void kfw_resp_json(kfw_response_t *resp, json_value_t *json) {
  if (!resp)
    return;

  char *body = json_stringify(json, false);
  if (!body)
    return;

  resp->body = body;
  resp->body_len = strlen(body);
  resp->body_allocated = true;
  resp->content_type = strdup("application/json");
  if (resp->status_code == 0)
    resp->status_code = 200;
}

void kfw_resp_text(kfw_response_t *resp, int status, const char *text) {
  if (!resp)
    return;

  resp->status_code = status;
  resp->body = strdup(text);
  resp->body_len = strlen(text);
  resp->body_allocated = true;
  resp->content_type = strdup("text/plain");
}

void kfw_resp_error(kfw_response_t *resp, int status, const char *message) {
  if (!resp)
    return;

  json_value_t *err = json_object_new();
  if (err) {
    json_object_set(err, "error", json_string(message));
    resp->status_code = status;
    kfw_resp_json(resp, err);
    json_free(err);
  }
}

bool kfw_resp_header(kfw_response_t *resp, const char *name,
                     const char *value) {
  if (!resp || !name || !value)
    return false;

  if (resp->header_count >= KFW_MAX_HEADERS)
    return false;

  resp->headers[resp->header_count].name = strdup(name);
  resp->headers[resp->header_count].value = strdup(value);
  if (!resp->headers[resp->header_count].name ||
      !resp->headers[resp->header_count].value) {
    free(resp->headers[resp->header_count].name);
    free(resp->headers[resp->header_count].value);
    return false;
  }

  resp->header_count++;
  return true;
}

void kfw_resp_status(kfw_response_t *resp, int status) {
  if (resp)
    resp->status_code = status;
}

/* ========== Request/Response lifecycle ========== */

void kfw_request_init(kfw_request_t *req) {
  if (req) {
    memset(req, 0, sizeof(kfw_request_t));
  }
}

void kfw_request_cleanup(kfw_request_t *req) {
  if (!req)
    return;

  /* Free path params */
  for (int i = 0; i < req->path_params_count; i++) {
    free(req->path_params[i].key);
    free(req->path_params[i].value);
  }

  /* Free query params */
  for (int i = 0; i < req->query_params_count; i++) {
    free(req->query_params[i].key);
    free(req->query_params[i].value);
  }

  /* Free headers */
  for (int i = 0; i < req->header_count; i++) {
    free(req->headers[i].name);
    free(req->headers[i].value);
  }

  /* Free JSON body */
  if (req->json) {
    json_free(req->json);
  }
}

void kfw_response_init(kfw_response_t *resp) {
  if (resp) {
    memset(resp, 0, sizeof(kfw_response_t));
    resp->status_code = 200;
  }
}

void kfw_response_cleanup(kfw_response_t *resp) {
  if (!resp)
    return;

  if (resp->body_allocated && resp->body) {
    free(resp->body);
  }
  free(resp->content_type);

  /* Free headers */
  for (int i = 0; i < resp->header_count; i++) {
    free(resp->headers[i].name);
    free(resp->headers[i].value);
  }
}

/* ========== Main request handler ========== */

kfw_error_t kfw_handle(kfw_app_t *app, const http_request_t *http_req,
                       const char *body, size_t body_len,
                       http_response_t *http_resp) {
  if (!app || !http_req || !http_resp)
    return KFW_ERR_INTERNAL;

  /* Initialize framework request/response */
  kfw_request_t req;
  kfw_response_t resp;
  kfw_request_init(&req);
  kfw_response_init(&resp);

  /* Copy HTTP method and path */
  strncpy(req.method, http_req->method, KFW_MAX_METHOD - 1);

  /* Extract path without query string */
  const char *query_start = strchr(http_req->path, '?');
  if (query_start) {
    size_t path_len = query_start - http_req->path;
    if (path_len >= KFW_MAX_PATH)
      path_len = KFW_MAX_PATH - 1;
    memcpy(req.path, http_req->path, path_len);
    req.path[path_len] = '\0';

    /* Parse query string */
    char *keys[KFW_MAX_PARAMS];
    char *values[KFW_MAX_PARAMS];
    int count =
        kfw_parse_query_string(query_start + 1, keys, values, KFW_MAX_PARAMS);
    for (int i = 0; i < count && i < KFW_MAX_PARAMS; i++) {
      req.query_params[i].key = keys[i];
      req.query_params[i].value = values[i];
    }
    req.query_params_count = count;
  } else {
    strncpy(req.path, http_req->path, KFW_MAX_PATH - 1);
  }

  /* Copy headers from HTTP request */
  for (int i = 0; i < http_req->header_count && i < KFW_MAX_HEADERS; i++) {
    req.headers[i].name = strdup(http_req->headers[i].name);
    req.headers[i].value = strdup(http_req->headers[i].value);
    req.header_count++;
  }

  /* Store body */
  req.body = body;
  req.body_len = body_len;

  /* Parse JSON body if present */
  if (body && body_len > 0) {
    /* Try to parse as JSON */
    json_ctx_t ctx;
    req.json = json_parse_n(body, body_len, &ctx);
    /* It's okay if parsing fails - not all requests are JSON */
  }

  /* Find matching route */
  kfw_route_t *matched_route = NULL;
  bool method_matched = false;

  for (int i = 0; i < app->route_count; i++) {
    kfw_route_t *route = &app->routes[i];

    /* Try to match pattern */
    kfw_request_t temp_req;
    kfw_request_init(&temp_req);

    if (kfw_match_route(route->pattern, req.path, &temp_req)) {
      method_matched = true;

      if (strcasecmp(route->method, req.method) == 0) {
        /* Full match - copy path params */
        for (int j = 0; j < temp_req.path_params_count; j++) {
          req.path_params[j].key = temp_req.path_params[j].key;
          req.path_params[j].value = temp_req.path_params[j].value;
        }
        req.path_params_count = temp_req.path_params_count;
        matched_route = route;
        break;
      }
    }

    /* Clean up temp request params if no match */
    for (int j = 0; j < temp_req.path_params_count; j++) {
      free(temp_req.path_params[j].key);
      free(temp_req.path_params[j].value);
    }
  }

  kfw_error_t result = KFW_OK;

  if (!matched_route) {
    if (method_matched) {
      kfw_resp_error(&resp, 405, "Method Not Allowed");
      result = KFW_ERR_METHOD_NOT_ALLOWED;
    } else {
      kfw_resp_error(&resp, 404, "Not Found");
      result = KFW_ERR_NOT_FOUND;
    }
  } else {
    /* Execute handler */
    result = matched_route->handler(&req, &resp);

    if (result != KFW_OK && resp.status_code < 400) {
      /* Handler returned error but didn't set error response */
      switch (result) {
      case KFW_ERR_BAD_REQUEST:
        kfw_resp_error(&resp, 400, "Bad Request");
        break;
      case KFW_ERR_UNAUTHORIZED:
        kfw_resp_error(&resp, 401, "Unauthorized");
        break;
      case KFW_ERR_FORBIDDEN:
        kfw_resp_error(&resp, 403, "Forbidden");
        break;
      case KFW_ERR_NOT_FOUND:
        kfw_resp_error(&resp, 404, "Not Found");
        break;
      default:
        kfw_resp_error(&resp, 500, "Internal Server Error");
        break;
      }
    }
  }

  /* Convert framework response to HTTP response */
  http_resp->status_code = resp.status_code;
  http_resp->content_type =
      resp.content_type ? resp.content_type : "text/plain";
  http_resp->body = resp.body;
  http_resp->body_len = resp.body_len;
  http_resp->keep_alive = http_req->keep_alive;

  /* Note: We don't call kfw_response_cleanup here because the body
   * is transferred to http_resp. The caller is responsible for
   * freeing http_resp->body after sending. */

  /* But we do clean up the request */
  kfw_request_cleanup(&req);

  return result;
}

/**
 * Kherashanu Application Layer - Implementation
 * REST API handlers for Blog + Portfolio platform
 */
#include "app.h"

#include <curl/curl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Global app context */
app_ctx_t *g_app = NULL;

/* ========== Model definitions ========== */

/* BlogPost ORM model */
const kfm_model_t BLOG_POST_MODEL = {
    .table_name = "blog_posts",
    .struct_size = sizeof(blog_post_t),
    .num_fields = 10,
    .pk_field_idx = 0,
    .fields = {{.name = "id",
                .type = KDB_TYPE_INTEGER,
                .flags = KFM_FLAG_PRIMARY_KEY | KFM_FLAG_AUTO_INCREMENT,
                .offset = offsetof(blog_post_t, id),
                .size = sizeof(int64_t)},
               {.name = "title",
                .type = KDB_TYPE_TEXT,
                .flags = KFM_FLAG_NOT_NULL,
                .offset = offsetof(blog_post_t, title),
                .size = sizeof(char *)},
               {.name = "url_slug",
                .type = KDB_TYPE_TEXT,
                .flags = KFM_FLAG_NOT_NULL | KFM_FLAG_UNIQUE,
                .offset = offsetof(blog_post_t, url_slug),
                .size = sizeof(char *)},
               {.name = "description",
                .type = KDB_TYPE_TEXT,
                .flags = 0,
                .offset = offsetof(blog_post_t, description),
                .size = sizeof(char *)},
               {.name = "summary",
                .type = KDB_TYPE_TEXT,
                .flags = 0,
                .offset = offsetof(blog_post_t, summary),
                .size = sizeof(char *)},
               {.name = "publish_date",
                .type = KDB_TYPE_TIMESTAMP,
                .flags = 0,
                .offset = offsetof(blog_post_t, publish_date),
                .size = sizeof(int64_t)},
               {.name = "category",
                .type = KDB_TYPE_TEXT,
                .flags = 0,
                .offset = offsetof(blog_post_t, category),
                .size = sizeof(char *)},
               {.name = "tags",
                .type = KDB_TYPE_TEXT,
                .flags = 0,
                .offset = offsetof(blog_post_t, tags),
                .size = sizeof(char *)},
               {.name = "content",
                .type = KDB_TYPE_TEXT,
                .flags = 0,
                .offset = offsetof(blog_post_t, content),
                .size = sizeof(char *)},
               {.name = "status",
                .type = KDB_TYPE_INTEGER,
                .flags = KFM_FLAG_NOT_NULL,
                .offset = offsetof(blog_post_t, status),
                .size = sizeof(int64_t)}}};

/* Session ORM model */
const kfm_model_t SESSION_MODEL = {
    .table_name = "sessions",
    .struct_size = sizeof(session_t),
    .num_fields = 5,
    .pk_field_idx = 0,
    .fields = {{.name = "id",
                .type = KDB_TYPE_INTEGER,
                .flags = KFM_FLAG_PRIMARY_KEY | KFM_FLAG_AUTO_INCREMENT,
                .offset = offsetof(session_t, id),
                .size = sizeof(int64_t)},
               {.name = "token",
                .type = KDB_TYPE_TEXT,
                .flags = KFM_FLAG_NOT_NULL | KFM_FLAG_UNIQUE,
                .offset = offsetof(session_t, token),
                .size = sizeof(char *)},
               {.name = "email",
                .type = KDB_TYPE_TEXT,
                .flags = KFM_FLAG_NOT_NULL,
                .offset = offsetof(session_t, email),
                .size = sizeof(char *)},
               {.name = "created_at",
                .type = KDB_TYPE_TIMESTAMP,
                .flags = KFM_FLAG_NOT_NULL,
                .offset = offsetof(session_t, created_at),
                .size = sizeof(int64_t)},
               {.name = "expires_at",
                .type = KDB_TYPE_TIMESTAMP,
                .flags = KFM_FLAG_NOT_NULL,
                .offset = offsetof(session_t, expires_at),
                .size = sizeof(int64_t)}}};

/* Export model definitions */
/* Models are now defined above */

/* ========== Helper functions ========== */

/* Generate a simple UUID-like token */
static char *generate_token(void) {
  static const char chars[] = "0123456789abcdef";
  char *token = malloc(37);
  if (!token)
    return NULL;

  srand((unsigned int)(time(NULL) ^ getpid()));

  for (int i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      token[i] = '-';
    } else {
      token[i] = chars[rand() % 16];
    }
  }
  token[36] = '\0';
  return token;
}

/* Load Google OAuth secrets from JSON file */
static int load_google_secrets(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Cannot open Google secrets file: %s\n", path);
    return -1;
  }

  /* Read file */
  char buf[4096];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);

  if (n <= 0)
    return -1;
  buf[n] = '\0';

  /* Parse JSON */
  json_ctx_t ctx;
  json_value_t *root = json_parse(buf, &ctx);
  if (!root) {
    fprintf(stderr, "Failed to parse Google secrets JSON\n");
    return -1;
  }

  json_value_t *web = json_object_get(root, "web");
  if (!web) {
    json_free(root);
    return -1;
  }

  json_value_t *client_id = json_object_get(web, "client_id");
  json_value_t *client_secret = json_object_get(web, "client_secret");
  json_value_t *redirect_uris = json_object_get(web, "redirect_uris");

  if (json_is_string(client_id)) {
    g_app->google_client_id = strdup(json_get_string(client_id));
  }
  if (json_is_string(client_secret)) {
    g_app->google_client_secret = strdup(json_get_string(client_secret));
  }
  if (json_is_array(redirect_uris) && json_array_len(redirect_uris) > 0) {
    json_value_t *uri = json_array_get(redirect_uris, 0);
    if (json_is_string(uri)) {
      g_app->google_redirect_uri = strdup(json_get_string(uri));
    }
  }

  json_free(root);
  return 0;
}

/* ========== Google OAuth Token Exchange ========== */

/* Curl write callback for collecting response data */
typedef struct {
  char *data;
  size_t size;
} curl_buffer_t;

static size_t oauth_write_callback(void *contents, size_t size, size_t nmemb,
                                   void *userp) {
  size_t realsize = size * nmemb;
  curl_buffer_t *buf = (curl_buffer_t *)userp;

  char *ptr = realloc(buf->data, buf->size + realsize + 1);
  if (!ptr)
    return 0;

  buf->data = ptr;
  memcpy(&(buf->data[buf->size]), contents, realsize);
  buf->size += realsize;
  buf->data[buf->size] = '\0';

  return realsize;
}

/**
 * Exchange OAuth authorization code for user email
 * Returns allocated email string on success, NULL on failure
 */
static char *google_oauth_exchange(const char *code, const char *redirect_uri) {
  if (!g_app->google_client_id || !g_app->google_client_secret) {
    fprintf(stderr, "Google OAuth secrets not configured\n");
    return NULL;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize curl\n");
    return NULL;
  }

  curl_buffer_t response = {.data = NULL, .size = 0};
  char *email = NULL;

  /* Step 1: Exchange code for tokens */
  char post_data[2048];
  snprintf(post_data, sizeof(post_data),
           "code=%s&client_id=%s&client_secret=%s&redirect_uri=%s&grant_type="
           "authorization_code",
           code, g_app->google_client_id, g_app->google_client_secret,
           redirect_uri);

  curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, oauth_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "Curl token request failed: %s\n", curl_easy_strerror(res));
    goto cleanup;
  }

  /* Parse token response */
  json_ctx_t ctx;
  json_value_t *token_json = json_parse(response.data, &ctx);
  if (!token_json) {
    fprintf(stderr, "Failed to parse token response\n");
    goto cleanup;
  }

  json_value_t *access_token = json_object_get(token_json, "access_token");
  if (!json_is_string(access_token)) {
    json_value_t *error = json_object_get(token_json, "error");
    if (json_is_string(error)) {
      fprintf(stderr, "OAuth error: %s\n", json_get_string(error));
    }
    json_free(token_json);
    goto cleanup;
  }

  const char *token = json_get_string(access_token);

  /* Step 2: Get user info using access token */
  free(response.data);
  response.data = NULL;
  response.size = 0;

  curl_easy_reset(curl);

  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, auth_header);

  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://www.googleapis.com/oauth2/v2/userinfo");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, oauth_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

  res = curl_easy_perform(curl);
  curl_slist_free_all(headers);
  json_free(token_json);

  if (res != CURLE_OK) {
    fprintf(stderr, "Curl userinfo request failed: %s\n",
            curl_easy_strerror(res));
    goto cleanup;
  }

  /* Parse userinfo response */
  json_value_t *userinfo = json_parse(response.data, &ctx);
  if (!userinfo) {
    fprintf(stderr, "Failed to parse userinfo response\n");
    goto cleanup;
  }

  json_value_t *email_val = json_object_get(userinfo, "email");
  if (json_is_string(email_val)) {
    email = strdup(json_get_string(email_val));
    printf("OAuth exchange successful for: %s\n", email);
  }

  json_free(userinfo);

cleanup:
  free(response.data);
  curl_easy_cleanup(curl);
  return email;
}

/* ========== Application lifecycle ========== */

int app_init(const char *db_path, const char *google_secrets_path) {
  /* Allocate context */
  g_app = calloc(1, sizeof(app_ctx_t));
  if (!g_app)
    return -1;

  /* Set admin email */
  const char *env_email = getenv("ADMIN_EMAIL");
  g_app->admin_email = env_email ? env_email : "kherashanu@gmail.com";

  /* Open database */
  kdb_error_t err = kdb_open(db_path, &g_app->db);
  if (err != KDB_OK) {
    fprintf(stderr, "Failed to open database: %s\n", db_path);
    free(g_app);
    g_app = NULL;
    return -1;
  }

  /* Initialize ORM */
  g_app->orm = kfm_init(g_app->db);
  if (!g_app->orm) {
    fprintf(stderr, "Failed to initialize ORM\n");
    kdb_close(g_app->db);
    free(g_app);
    g_app = NULL;
    return -1;
  }

  /* Create tables if they don't exist */
  kfm_error_t orm_err = kfm_create_table(g_app->orm, &BLOG_POST_MODEL);
  if (orm_err != KFM_OK && orm_err != KFM_ERR_DB) {
    fprintf(stderr,
            "Warning: Could not create blog_posts table (may already exist)\n");
  }

  orm_err = kfm_create_table(g_app->orm, &SESSION_MODEL);
  if (orm_err != KFM_OK && orm_err != KFM_ERR_DB) {
    fprintf(stderr,
            "Warning: Could not create sessions table (may already exist)\n");
  }

  /* Load Google OAuth secrets */
  if (google_secrets_path && load_google_secrets(google_secrets_path) < 0) {
    fprintf(stderr, "Warning: Could not load Google OAuth secrets\n");
  }

  /* Create framework */
  g_app->framework = kfw_app_create();
  if (!g_app->framework) {
    fprintf(stderr, "Failed to create framework\n");
    kfm_destroy(g_app->orm);
    kdb_close(g_app->db);
    free(g_app);
    g_app = NULL;
    return -1;
  }

  /* Register routes */
  /* Public blog APIs */
  KFW_GET(g_app->framework, "/api/blogs", api_list_blogs);
  KFW_GET(g_app->framework, "/api/blogs/latest", api_latest_blog);
  KFW_GET(g_app->framework, "/api/blog/:slug", api_get_blog);

  /* Auth APIs */
  KFW_POST(g_app->framework, "/api/auth/google", api_auth_google);
  KFW_GET(g_app->framework, "/api/auth/me", api_auth_me);
  KFW_POST(g_app->framework, "/api/auth/logout", api_auth_logout);

  /* Admin APIs */
  KFW_GET(g_app->framework, "/api/admin/blogs", api_admin_list_blogs);
  KFW_POST(g_app->framework, "/api/admin/blog", api_admin_create_blog);
  KFW_PUT(g_app->framework, "/api/admin/blog/:slug", api_admin_update_blog);
  KFW_DELETE(g_app->framework, "/api/admin/blog/:slug", api_admin_delete_blog);
  KFW_GET(g_app->framework, "/api/admin/stats", api_admin_stats);

  printf("Application initialized. Admin email: %s\n", g_app->admin_email);
  return 0;
}

void app_cleanup(void) {
  if (!g_app)
    return;

  if (g_app->framework)
    kfw_app_destroy(g_app->framework);
  if (g_app->orm)
    kfm_destroy(g_app->orm);
  if (g_app->db)
    kdb_close(g_app->db);

  free((void *)g_app->google_client_id);
  free((void *)g_app->google_client_secret);
  free((void *)g_app->google_redirect_uri);

  free(g_app);
  g_app = NULL;
}

kfw_app_t *app_get_framework(void) { return g_app ? g_app->framework : NULL; }

/* ========== Authentication helpers ========== */

session_t *auth_validate_session(kfw_request_t *req) {
  const char *auth_header = kfw_req_header(req, "Authorization");
  if (!auth_header)
    return NULL;

  /* Expect "Bearer <token>" */
  if (strncmp(auth_header, "Bearer ", 7) != 0)
    return NULL;

  const char *token = auth_header + 7;

  /* Find session by token */
  char where[256];
  snprintf(where, sizeof(where), "token = '%s'", token);

  session_t *session =
      (session_t *)kfm_find_one(g_app->orm, &SESSION_MODEL, where);
  if (!session)
    return NULL;

  /* Check expiration */
  time_t now = time(NULL);
  if (session->expires_at < now) {
    kfm_delete(g_app->orm, &SESSION_MODEL, session);
    kfm_free(&SESSION_MODEL, session);
    return NULL;
  }

  return session;
}

bool auth_is_authenticated(kfw_request_t *req) {
  session_t *session = auth_validate_session(req);
  if (session) {
    kfm_free(&SESSION_MODEL, session);
    return true;
  }
  return false;
}

session_t *auth_create_session(const char *email) {
  session_t *session = (session_t *)kfm_new(&SESSION_MODEL);
  if (!session)
    return NULL;

  session->token = generate_token();
  session->email = strdup(email);
  session->created_at = time(NULL);
  session->expires_at = session->created_at + (24 * 60 * 60); /* 24 hours */

  if (kfm_insert(g_app->orm, &SESSION_MODEL, session) != KFM_OK) {
    kfm_free(&SESSION_MODEL, session);
    return NULL;
  }

  return session;
}

void auth_destroy_session(const char *token) {
  char where[256];
  snprintf(where, sizeof(where), "token = '%s'", token);

  session_t *session =
      (session_t *)kfm_find_one(g_app->orm, &SESSION_MODEL, where);
  if (session) {
    kfm_delete(g_app->orm, &SESSION_MODEL, session);
    kfm_free(&SESSION_MODEL, session);
  }
}

/* ========== Public API handlers ========== */

kfw_error_t api_list_blogs(kfw_request_t *req, kfw_response_t *resp) {
  (void)req;

  /* Get only published blogs */
  char where[64];
  snprintf(where, sizeof(where), "status = %d", BLOG_STATUS_PUBLISHED);

  kfm_list_t *blogs = kfm_find_where(g_app->orm, &BLOG_POST_MODEL, where);
  if (!blogs) {
    kfw_resp_error(resp, 500, "Database error");
    return KFW_ERR_INTERNAL;
  }

  json_value_t *arr = kfm_list_to_json(blogs);
  kfm_list_free(blogs);

  kfw_resp_json(resp, arr);
  json_free(arr);

  return KFW_OK;
}

kfw_error_t api_get_blog(kfw_request_t *req, kfw_response_t *resp) {
  const char *slug = kfw_req_param(req, "slug");
  if (!slug) {
    kfw_resp_error(resp, 400, "Missing slug parameter");
    return KFW_ERR_BAD_REQUEST;
  }

  /* Find by slug */
  char where[512];
  snprintf(where, sizeof(where), "url_slug = '%s' AND status = %d", slug,
           BLOG_STATUS_PUBLISHED);

  blog_post_t *post =
      (blog_post_t *)kfm_find_one(g_app->orm, &BLOG_POST_MODEL, where);
  if (!post) {
    kfw_resp_error(resp, 404, "Blog post not found");
    return KFW_ERR_NOT_FOUND;
  }

  json_value_t *json = kfm_to_json(&BLOG_POST_MODEL, post);
  kfm_free(&BLOG_POST_MODEL, post);

  kfw_resp_json(resp, json);
  json_free(json);

  return KFW_OK;
}

kfw_error_t api_latest_blog(kfw_request_t *req, kfw_response_t *resp) {
  (void)req;

  /* Get published blogs ordered by date - simplified approach */
  char where[64];
  snprintf(where, sizeof(where), "status = %d", BLOG_STATUS_PUBLISHED);

  kfm_list_t *blogs = kfm_find_where(g_app->orm, &BLOG_POST_MODEL, where);
  if (!blogs || blogs->count == 0) {
    kfm_list_free(blogs);
    kfw_resp_error(resp, 404, "No published blogs");
    return KFW_ERR_NOT_FOUND;
  }

  /* Find the one with the latest publish_date */
  blog_post_t *latest = NULL;
  int64_t max_date = 0;
  for (int i = 0; i < blogs->count; i++) {
    blog_post_t *post = (blog_post_t *)blogs->items[i];
    if (post->publish_date > max_date) {
      max_date = post->publish_date;
      latest = post;
    }
  }

  if (!latest) {
    kfm_list_free(blogs);
    kfw_resp_error(resp, 404, "No published blogs");
    return KFW_ERR_NOT_FOUND;
  }

  json_value_t *json = kfm_to_json(&BLOG_POST_MODEL, latest);
  kfm_list_free(blogs);

  kfw_resp_json(resp, json);
  json_free(json);

  return KFW_OK;
}

/* ========== Auth API handlers ========== */

kfw_error_t api_auth_google(kfw_request_t *req, kfw_response_t *resp) {
  json_value_t *code_val = kfw_req_json_get(req, "code");
  json_value_t *redirect_uri_val = kfw_req_json_get(req, "redirect_uri");

  const char *email = NULL;
  char *oauth_email = NULL; /* Allocated email from OAuth exchange */

  /* Production mode: exchange OAuth code for email */
  if (json_is_string(code_val)) {
    const char *code = json_get_string(code_val);

    /* Use provided redirect_uri or default from secrets */
    const char *redirect_uri = g_app->google_redirect_uri;
    if (json_is_string(redirect_uri_val)) {
      redirect_uri = json_get_string(redirect_uri_val);
    }

    if (!redirect_uri) {
      const char *env_redirect = getenv("GOOGLE_REDIRECT_URI");
      if (env_redirect) {
        redirect_uri = env_redirect;
      } else {
        redirect_uri = "http://localhost:3000/auth/callback";
      }
    }

    oauth_email = google_oauth_exchange(code, redirect_uri);
    if (!oauth_email) {
      kfw_resp_error(
          resp, 401,
          "OAuth token exchange failed - invalid code or configuration");
      return KFW_ERR_UNAUTHORIZED;
    }
    email = oauth_email;
  } else {
    kfw_resp_error(resp, 400, "Missing OAuth code");
    return KFW_ERR_BAD_REQUEST;
  }

  /* Check whitelist */
  if (strcmp(email, g_app->admin_email) != 0) {
    free(oauth_email);
    kfw_resp_error(resp, 403, "Email not authorized for admin access");
    return KFW_ERR_FORBIDDEN;
  }

  /* Create session */
  session_t *session = auth_create_session(email);
  free(oauth_email); /* Done with the email string */

  if (!session) {
    kfw_resp_error(resp, 500, "Failed to create session");
    return KFW_ERR_INTERNAL;
  }

  /* Return session token */
  json_value_t *result = json_object_new();
  json_object_set(result, "token", json_string(session->token));
  json_object_set(result, "email", json_string(session->email));
  json_object_set(result, "expires_at", json_int(session->expires_at));

  kfm_free(&SESSION_MODEL, session);

  kfw_resp_json(resp, result);
  json_free(result);

  return KFW_OK;
}

kfw_error_t api_auth_me(kfw_request_t *req, kfw_response_t *resp) {
  session_t *session = auth_validate_session(req);
  if (!session) {
    kfw_resp_error(resp, 401, "Not authenticated");
    return KFW_ERR_UNAUTHORIZED;
  }

  json_value_t *result = json_object_new();
  json_object_set(result, "email", json_string(session->email));
  json_object_set(result, "expires_at", json_int(session->expires_at));

  kfm_free(&SESSION_MODEL, session);

  kfw_resp_json(resp, result);
  json_free(result);

  return KFW_OK;
}

kfw_error_t api_auth_logout(kfw_request_t *req, kfw_response_t *resp) {
  const char *auth_header = kfw_req_header(req, "Authorization");
  if (auth_header && strncmp(auth_header, "Bearer ", 7) == 0) {
    auth_destroy_session(auth_header + 7);
  }

  json_value_t *result = json_object_new();
  json_object_set(result, "success", json_bool(true));

  kfw_resp_json(resp, result);
  json_free(result);

  return KFW_OK;
}

/* ========== Admin API handlers ========== */

/* Macro to check authentication */
#define REQUIRE_AUTH(req, resp)                                                \
  do {                                                                         \
    if (!auth_is_authenticated(req)) {                                         \
      kfw_resp_error(resp, 401, "Authentication required");                    \
      return KFW_ERR_UNAUTHORIZED;                                             \
    }                                                                          \
  } while (0)

kfw_error_t api_admin_list_blogs(kfw_request_t *req, kfw_response_t *resp) {
  REQUIRE_AUTH(req, resp);

  /* Get all blogs (drafts + published) */
  kfm_list_t *blogs = kfm_find_all(g_app->orm, &BLOG_POST_MODEL);
  if (!blogs) {
    kfw_resp_error(resp, 500, "Database error");
    return KFW_ERR_INTERNAL;
  }

  json_value_t *arr = kfm_list_to_json(blogs);
  kfm_list_free(blogs);

  kfw_resp_json(resp, arr);
  json_free(arr);

  return KFW_OK;
}

kfw_error_t api_admin_create_blog(kfw_request_t *req, kfw_response_t *resp) {
  REQUIRE_AUTH(req, resp);

  if (!req->json) {
    kfw_resp_error(resp, 400, "Invalid JSON body");
    return KFW_ERR_BAD_REQUEST;
  }

  /* Create new blog post */
  blog_post_t *post = (blog_post_t *)kfm_new(&BLOG_POST_MODEL);
  if (!post) {
    kfw_resp_error(resp, 500, "Memory allocation failed");
    return KFW_ERR_INTERNAL;
  }

  /* Populate from JSON */
  kfm_from_json(&BLOG_POST_MODEL, post, req->json);

  /* Set defaults */
  if (post->publish_date == 0) {
    post->publish_date = time(NULL);
  }

  /* Validate required fields */
  if (!post->title || !post->url_slug) {
    kfm_free(&BLOG_POST_MODEL, post);
    kfw_resp_error(resp, 400, "Missing required fields: title, url_slug");
    return KFW_ERR_BAD_REQUEST;
  }

  /* Insert into database */
  if (kfm_insert(g_app->orm, &BLOG_POST_MODEL, post) != KFM_OK) {
    kfm_free(&BLOG_POST_MODEL, post);
    kfw_resp_error(resp, 500, "Failed to create blog post");
    return KFW_ERR_INTERNAL;
  }

  json_value_t *result = kfm_to_json(&BLOG_POST_MODEL, post);
  kfm_free(&BLOG_POST_MODEL, post);

  resp->status_code = 201;
  kfw_resp_json(resp, result);
  json_free(result);

  return KFW_OK;
}

kfw_error_t api_admin_update_blog(kfw_request_t *req, kfw_response_t *resp) {
  REQUIRE_AUTH(req, resp);

  const char *slug = kfw_req_param(req, "slug");
  if (!slug) {
    kfw_resp_error(resp, 400, "Missing slug parameter");
    return KFW_ERR_BAD_REQUEST;
  }

  if (!req->json) {
    kfw_resp_error(resp, 400, "Invalid JSON body");
    return KFW_ERR_BAD_REQUEST;
  }

  /* Find existing post */
  char where[512];
  snprintf(where, sizeof(where), "url_slug = '%s'", slug);

  blog_post_t *post =
      (blog_post_t *)kfm_find_one(g_app->orm, &BLOG_POST_MODEL, where);
  if (!post) {
    kfw_resp_error(resp, 404, "Blog post not found");
    return KFW_ERR_NOT_FOUND;
  }

  /* Update from JSON (preserves ID) */
  int64_t id = post->id;
  kfm_from_json(&BLOG_POST_MODEL, post, req->json);
  post->id = id; /* Restore ID */

  /* Update in database */
  if (kfm_update(g_app->orm, &BLOG_POST_MODEL, post) != KFM_OK) {
    kfm_free(&BLOG_POST_MODEL, post);
    kfw_resp_error(resp, 500, "Failed to update blog post");
    return KFW_ERR_INTERNAL;
  }

  json_value_t *result = kfm_to_json(&BLOG_POST_MODEL, post);
  kfm_free(&BLOG_POST_MODEL, post);

  kfw_resp_json(resp, result);
  json_free(result);

  return KFW_OK;
}

kfw_error_t api_admin_delete_blog(kfw_request_t *req, kfw_response_t *resp) {
  REQUIRE_AUTH(req, resp);

  const char *slug = kfw_req_param(req, "slug");
  if (!slug) {
    kfw_resp_error(resp, 400, "Missing slug parameter");
    return KFW_ERR_BAD_REQUEST;
  }

  /* Find existing post */
  char where[512];
  snprintf(where, sizeof(where), "url_slug = '%s'", slug);

  blog_post_t *post =
      (blog_post_t *)kfm_find_one(g_app->orm, &BLOG_POST_MODEL, where);
  if (!post) {
    kfw_resp_error(resp, 404, "Blog post not found");
    return KFW_ERR_NOT_FOUND;
  }

  /* Delete from database */
  if (kfm_delete(g_app->orm, &BLOG_POST_MODEL, post) != KFM_OK) {
    kfm_free(&BLOG_POST_MODEL, post);
    kfw_resp_error(resp, 500, "Failed to delete blog post");
    return KFW_ERR_INTERNAL;
  }

  kfm_free(&BLOG_POST_MODEL, post);

  json_value_t *result = json_object_new();
  json_object_set(result, "success", json_bool(true));

  kfw_resp_json(resp, result);
  json_free(result);

  return KFW_OK;
}

kfw_error_t api_admin_stats(kfw_request_t *req, kfw_response_t *resp) {
  REQUIRE_AUTH(req, resp);

  /* Get blog counts */
  int total = kfm_count(g_app->orm, &BLOG_POST_MODEL, NULL);

  char where[64];
  snprintf(where, sizeof(where), "status = %d", BLOG_STATUS_PUBLISHED);
  int published = kfm_count(g_app->orm, &BLOG_POST_MODEL, where);

  snprintf(where, sizeof(where), "status = %d", BLOG_STATUS_DRAFT);
  int drafts = kfm_count(g_app->orm, &BLOG_POST_MODEL, where);

  json_value_t *result = json_object_new();
  json_object_set(result, "total_posts", json_int(total));
  json_object_set(result, "published", json_int(published));
  json_object_set(result, "drafts", json_int(drafts));
  /* TODO: Add visit counts when logging is implemented */
  json_object_set(result, "visits", json_int(0));

  kfw_resp_json(resp, result);
  json_free(result);

  return KFW_OK;
}

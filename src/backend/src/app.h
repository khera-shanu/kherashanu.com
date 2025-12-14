/**
 * Kherashanu Application Layer
 * Integrates web framework with ORM for Blog + Portfolio platform
 */
#ifndef KFW_APP_H
#define KFW_APP_H

#include "db/db.h"
#include "framework/framework.h"
#include "orm/orm.h"
#include "json/json.h"
#include <stdint.h>
#include <time.h>

/* Blog post status */
typedef enum { BLOG_STATUS_DRAFT = 0, BLOG_STATUS_PUBLISHED = 1 } blog_status_t;

/* Blog post model - matches required schema */
typedef struct {
  int64_t id;           /* Unique identifier (auto-increment) */
  char *title;          /* Post title */
  char *url_slug;       /* URL-friendly slug (unique) */
  char *description;    /* Meta description (SEO) */
  char *summary;        /* Markdown summary/excerpt */
  int64_t publish_date; /* Unix timestamp */
  char *category;       /* Category name */
  char *tags;           /* Comma-separated tags */
  char *content;        /* Markdown content blob */
  int64_t status;       /* DRAFT=0, PUBLISHED=1 */
} blog_post_t;

/* Session for authentication */
typedef struct {
  int64_t id;
  char *token;        /* Session token (UUID) */
  char *email;        /* User email */
  int64_t created_at; /* Creation timestamp */
  int64_t expires_at; /* Expiration timestamp */
} session_t;

/* Application context */
typedef struct {
  kdb_t *db;
  kfm_ctx_t *orm;
  kfw_app_t *framework;

  /* Configuration */
  const char *google_client_id;
  const char *google_client_secret;
  const char *google_redirect_uri;
  const char *admin_email; /* Whitelisted admin email */
} app_ctx_t;

/* Global app context (initialized in main) */
extern app_ctx_t *g_app;

/* Model definitions */
extern const kfm_model_t BLOG_POST_MODEL;
extern const kfm_model_t SESSION_MODEL;

/* ========== Application lifecycle ========== */

/**
 * Initialize application
 * @param db_path Path to database file
 * @param google_secrets_path Path to Google OAuth secrets JSON
 * @return 0 on success, -1 on error
 */
int app_init(const char *db_path, const char *google_secrets_path);

/**
 * Cleanup application resources
 */
void app_cleanup(void);

/**
 * Get the framework app for routing integration
 */
kfw_app_t *app_get_framework(void);

/* ========== Public API handlers ========== */

/* GET /api/blogs - List published blogs */
kfw_error_t api_list_blogs(kfw_request_t *req, kfw_response_t *resp);

/* GET /api/blog/:slug - Get single blog */
kfw_error_t api_get_blog(kfw_request_t *req, kfw_response_t *resp);

/* GET /api/blogs/latest - Get latest blog post */
kfw_error_t api_latest_blog(kfw_request_t *req, kfw_response_t *resp);

/* ========== Auth API handlers ========== */

/* POST /api/auth/google - Handle Google OAuth callback */
kfw_error_t api_auth_google(kfw_request_t *req, kfw_response_t *resp);

/* GET /api/auth/me - Get current session */
kfw_error_t api_auth_me(kfw_request_t *req, kfw_response_t *resp);

/* POST /api/auth/logout - Logout */
kfw_error_t api_auth_logout(kfw_request_t *req, kfw_response_t *resp);

/* ========== Admin API handlers (protected) ========== */

/* GET /api/admin/blogs - List all blogs (drafts + published) */
kfw_error_t api_admin_list_blogs(kfw_request_t *req, kfw_response_t *resp);

/* POST /api/admin/blog - Create new blog */
kfw_error_t api_admin_create_blog(kfw_request_t *req, kfw_response_t *resp);

/* PUT /api/admin/blog/:slug - Update blog */
kfw_error_t api_admin_update_blog(kfw_request_t *req, kfw_response_t *resp);

/* DELETE /api/admin/blog/:slug - Delete blog */
kfw_error_t api_admin_delete_blog(kfw_request_t *req, kfw_response_t *resp);

/* GET /api/admin/stats - Get basic analytics */
kfw_error_t api_admin_stats(kfw_request_t *req, kfw_response_t *resp);

/* ========== Authentication helpers ========== */

/**
 * Validate session token from Authorization header
 * @return Session if valid, NULL otherwise
 */
session_t *auth_validate_session(kfw_request_t *req);

/**
 * Check if request is authenticated
 */
bool auth_is_authenticated(kfw_request_t *req);

/**
 * Create a new session for email
 */
session_t *auth_create_session(const char *email);

/**
 * Destroy a session
 */
void auth_destroy_session(const char *token);

#endif /* KFW_APP_H */

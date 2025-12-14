#include "app.h"
#include "db/db.h"
#include "orm/orm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <db_path> <token>\n", argv[0]);
    return 1;
  }

  const char *db_path = argv[1];
  const char *token = argv[2];

  kdb_t *db;
  if (kdb_open(db_path, &db) != KDB_OK) {
    fprintf(stderr, "Failed to open DB\n");
    return 1;
  }

  kfm_ctx_t *ctx = kfm_init(db);

  /* We need to use the models from app.c, but they are static there.
     We'll redefine the SESSION_MODEL here for this utility since we know the
     schema. */

  kfm_model_t *session_model = kfm_model_create("sessions", sizeof(session_t));
  kfm_model_add_field(session_model, "id", KDB_TYPE_INTEGER,
                      offsetof(session_t, id), sizeof(int64_t),
                      KFM_FLAG_PRIMARY_KEY | KFM_FLAG_AUTO_INCREMENT);
  kfm_model_add_field(session_model, "token", KDB_TYPE_TEXT,
                      offsetof(session_t, token), 0,
                      KFM_FLAG_NOT_NULL | KFM_FLAG_UNIQUE);
  kfm_model_add_field(session_model, "email", KDB_TYPE_TEXT,
                      offsetof(session_t, email), 0, KFM_FLAG_NOT_NULL);
  kfm_model_add_field(session_model, "created_at", KDB_TYPE_TIMESTAMP,
                      offsetof(session_t, created_at), sizeof(int64_t),
                      KFM_FLAG_NOT_NULL);
  kfm_model_add_field(session_model, "expires_at", KDB_TYPE_TIMESTAMP,
                      offsetof(session_t, expires_at), sizeof(int64_t),
                      KFM_FLAG_NOT_NULL);

  /* Check if table exists, create if not */
  kfm_create_table(ctx, session_model);

  session_t *s = kfm_new(session_model);
  s->token = strdup(token);
  s->email = strdup("test@example.com");
  s->created_at = time(NULL);
  s->expires_at = time(NULL) + 3600;

  if (kfm_save(ctx, session_model, s) != KFM_OK) {
    fprintf(stderr, "Failed to save session\n");
    kfm_free(session_model, s);
    return 1;
  }

  printf("Session created: %s\n", token);

  kfm_free(session_model, s);
  kfm_model_free(session_model); // Free our local model definition
  kfm_destroy(ctx);
  kdb_close(db);
  return 0;
}

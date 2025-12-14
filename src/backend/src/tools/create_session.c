/**
 * Kherashanu Tool - Create Admin Session
 * Generates a valid session token for kherashanu@gmail.com
 */
#include "../app.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  const char *db_path = "kherashanu.db";
  const char *email = "kherashanu@gmail.com";

  if (argc > 1) {
    db_path = argv[1];
  }

  printf("Opening database: %s\n", db_path);

  /* Initialize app (creates tables etc) */
  if (app_init(db_path, NULL) < 0) {
    fprintf(stderr, "Failed to initialize app\n");
    return 1;
  }

  /* Create session */
  session_t *session = auth_create_session(email);
  if (!session) {
    fprintf(stderr, "Failed to create session\n");
    app_cleanup();
    return 1;
  }

  printf("Session created successfully!\n");
  printf("Token: %s\n", session->token);
  printf("Email: %s\n", session->email);
  printf("Expires: %ld\n", (long)session->expires_at);

  /* Cleanup */
  kfm_free(&SESSION_MODEL, session);
  app_cleanup();

  return 0;
}

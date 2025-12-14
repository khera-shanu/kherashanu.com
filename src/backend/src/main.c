#include "app.h"
#include "server.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void print_usage(const char *prog) {
  printf("Usage: %s [OPTIONS]\n\n", prog);
  printf("Kherashanu Portfolio + Blog Platform\n\n");
  printf("Options:\n");
  printf("  -p, --port PORT       HTTP port (default: 3000)\n");
  printf("  -s, --tls-port PORT   HTTPS port (default: 0, disabled)\n");
  printf("  -c, --cert PATH       TLS certificate file\n");
  printf("  -k, --key PATH        TLS private key file\n");
  printf("  -w, --webroot PATH    Static files directory (default: "
         "../frontend/dist)\n");
  printf("  -d, --database PATH   Database file (default: data/blog.db)\n");
  printf("  -g, --google PATH     Google OAuth secrets JSON (default: "
         "google_oauth_secrets.json)\n");
  printf("  -h, --help            Show this help message\n");
}

int main(int argc, char *argv[]) {
  server_config_t config = {.http_port = 3000,
                            .https_port = 0, /* Disabled by default */
                            .cert_path = "",
                            .key_path = "",
                            .webroot = "../frontend/dist",
                            .use_tls = false};

  char db_path[512] = "data/blog.db";
  char google_secrets_path[512] = "google_oauth_secrets.json";

  static struct option long_options[] = {
      {"port", required_argument, 0, 'p'},
      {"tls-port", required_argument, 0, 's'},
      {"cert", required_argument, 0, 'c'},
      {"key", required_argument, 0, 'k'},
      {"webroot", required_argument, 0, 'w'},
      {"database", required_argument, 0, 'd'},
      {"google", required_argument, 0, 'g'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "p:s:c:k:w:d:g:h", long_options,
                            NULL)) != -1) {
    switch (opt) {
    case 'p':
      config.http_port = atoi(optarg);
      break;
    case 's':
      config.https_port = atoi(optarg);
      break;
    case 'c':
      strncpy(config.cert_path, optarg, sizeof(config.cert_path) - 1);
      break;
    case 'k':
      strncpy(config.key_path, optarg, sizeof(config.key_path) - 1);
      break;
    case 'w':
      strncpy(config.webroot, optarg, sizeof(config.webroot) - 1);
      break;
    case 'd':
      strncpy(db_path, optarg, sizeof(db_path) - 1);
      break;
    case 'g':
      strncpy(google_secrets_path, optarg, sizeof(google_secrets_path) - 1);
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  // Enable TLS if both cert and key are provided
  if (config.cert_path[0] && config.key_path[0]) {
    config.use_tls = true;
  } else if (config.https_port > 0) {
    printf("Note: HTTPS disabled (no certificate/key provided)\n");
    config.https_port = 0;
  }

  // Resolve webroot to absolute path if relative
  if (config.webroot[0] != '/') {
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd))) {
      char resolved[WEBROOT_PATH_MAX];
      snprintf(resolved, sizeof(resolved), "%s/%s", cwd, config.webroot);
      strncpy(config.webroot, resolved, sizeof(config.webroot) - 1);
    }
  }

  // Check webroot exists
  if (access(config.webroot, R_OK) != 0) {
    fprintf(stderr, "Error: Cannot access webroot: %s\n", config.webroot);
    return 1;
  }

  // Create data directory if needed
  if (strncmp(db_path, "data/", 5) == 0) {
    if (access("data", F_OK) != 0) {
      if (mkdir("data", 0755) != 0) {
        fprintf(stderr, "Warning: Could not create data directory\n");
      }
    }
  }

  printf("=================================================\n");
  printf("  Kherashanu Portfolio + Blog Platform\n");
  printf("=================================================\n\n");

  // Initialize application (database, ORM, framework)
  if (app_init(db_path, google_secrets_path) < 0) {
    fprintf(stderr, "Failed to initialize application\n");
    return 1;
  }

  if (server_init(&config) < 0) {
    fprintf(stderr, "Failed to initialize server\n");
    app_cleanup();
    return 1;
  }

  int ret = server_run();

  server_shutdown();
  app_cleanup();

  return ret;
}

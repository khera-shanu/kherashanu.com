/**
 * Kherashanu Tool - Dump Blog Posts
 * Inspects DB state
 */
#include "../app.h"
#include <stdio.h>

int main(int argc, char **argv) {
  const char *db_path = "data/blog.db";
  if (argc > 1)
    db_path = argv[1];

  if (app_init(db_path, NULL) < 0)
    return 1;

  printf("Dumping blog_posts table...\n");

  kfm_list_t *list = kfm_find_all(g_app->orm, &BLOG_POST_MODEL);
  if (!list) {
    printf("Failed to find all. Error: %s\n", kdb_error_msg(g_app->db));
    return 1;
  }

  printf("Found %d rows.\n", list->count);

  for (int i = 0; i < list->count; i++) {
    blog_post_t *p = (blog_post_t *)list->items[i];
    printf("[%d] ID=%lld, Slug='%s', Title='%s'\n", i, (long long)p->id,
           p->url_slug, p->title);
  }

  kfm_list_free(list);
  app_cleanup();
  return 0;
}

/**
 * ORM Test Suite
 */
#include "../src/db/db.h"
#include "../src/json/json.h"
#include "../src/orm/orm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  do {                                                                         \
    printf("  Testing %s... ", name);                                          \
    tests_run++;                                                               \
  } while (0)

#define PASS()                                                                 \
  do {                                                                         \
    printf("\033[32mPASS\033[0m\n");                                           \
    tests_passed++;                                                            \
  } while (0)

#define FAIL(msg)                                                              \
  do {                                                                         \
    printf("\033[31mFAIL\033[0m: %s\n", msg);                                  \
  } while (0)

#define ASSERT(cond, msg)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      FAIL(msg);                                                               \
      return;                                                                  \
    }                                                                          \
  } while (0)

/* ========== Test model: Blog ========== */

typedef struct {
  int64_t id;
  char *title;
  char *url_slug;
  char *content;
  int64_t status;
  int64_t created_at;
} test_blog_t;

/* Model metadata */
static kfm_model_t blog_model = {
    .table_name = "blogs",
    .struct_size = sizeof(test_blog_t),
    .num_fields = 6,
    .fields =
        {
            {.name = "id",
             .type = KDB_TYPE_INTEGER,
             .flags = KFM_FLAG_PRIMARY_KEY,
             .offset = offsetof(test_blog_t, id),
             .size = sizeof(int64_t)},
            {.name = "title",
             .type = KDB_TYPE_TEXT,
             .flags = KFM_FLAG_NOT_NULL,
             .offset = offsetof(test_blog_t, title),
             .size = sizeof(char *)},
            {.name = "url_slug",
             .type = KDB_TYPE_TEXT,
             .flags = KFM_FLAG_UNIQUE,
             .offset = offsetof(test_blog_t, url_slug),
             .size = sizeof(char *)},
            {.name = "content",
             .type = KDB_TYPE_TEXT,
             .flags = 0,
             .offset = offsetof(test_blog_t, content),
             .size = sizeof(char *)},
            {.name = "status",
             .type = KDB_TYPE_INTEGER,
             .flags = 0,
             .offset = offsetof(test_blog_t, status),
             .size = sizeof(int64_t)},
            {.name = "created_at",
             .type = KDB_TYPE_TIMESTAMP,
             .flags = 0,
             .offset = offsetof(test_blog_t, created_at),
             .size = sizeof(int64_t)},
        },
    .pk_field_idx = 0};

/* ========== Test fixtures ========== */

static kdb_t *g_db = NULL;
static kfm_ctx_t *g_ctx = NULL;

static void setup(void) {
  unlink("/tmp/test_orm.db");
  unlink("/tmp/test_orm.db.wal");

  kdb_error_t err = kdb_open("/tmp/test_orm.db", &g_db);
  if (err != KDB_OK) {
    fprintf(stderr, "Failed to open database\n");
    exit(1);
  }

  g_ctx = kfm_init(g_db);
  if (!g_ctx) {
    fprintf(stderr, "Failed to create ORM context\n");
    exit(1);
  }
}

static void teardown(void) {
  kfm_destroy(g_ctx);
  kdb_close(g_db);
  unlink("/tmp/test_orm.db");
  unlink("/tmp/test_orm.db.wal");
}

/* ========== Table tests ========== */

static void test_create_table(void) {
  TEST("create table");

  kfm_error_t err = kfm_create_table(g_ctx, &blog_model);
  ASSERT(err == KFM_OK, "Create table failed");

  /* Verify with raw query */
  kdb_result_t *result = kfm_raw_query(g_ctx, "SELECT * FROM blogs");
  ASSERT(result != NULL, "Table query failed");
  ASSERT(result->num_rows == 0, "Table not empty");
  kdb_result_free(result);

  PASS();
}

/* ========== CRUD tests ========== */

static void test_insert(void) {
  TEST("insert");

  test_blog_t blog = {.id = 1,
                      .title = strdup("First Post"),
                      .url_slug = strdup("first-post"),
                      .content = strdup("Hello world!"),
                      .status = 1,
                      .created_at = 1700000000};

  kfm_error_t err = kfm_insert(g_ctx, &blog_model, &blog);
  ASSERT(err == KFM_OK, "Insert failed");

  /* Verify */
  int count = kfm_count(g_ctx, &blog_model, NULL);
  ASSERT(count == 1, "Count wrong after insert");

  free(blog.title);
  free(blog.url_slug);
  free(blog.content);
  PASS();
}

static void test_find_by_id(void) {
  TEST("find by ID");

  test_blog_t *blog = kfm_find_by_id(g_ctx, &blog_model, 1);
  ASSERT(blog != NULL, "Find by ID returned NULL");
  ASSERT(blog->id == 1, "Wrong ID");
  ASSERT(strcmp(blog->title, "First Post") == 0, "Wrong title");
  ASSERT(strcmp(blog->url_slug, "first-post") == 0, "Wrong slug");

  kfm_free(&blog_model, blog);
  PASS();
}

static void test_update(void) {
  TEST("update");

  test_blog_t *blog = kfm_find_by_id(g_ctx, &blog_model, 1);
  ASSERT(blog != NULL, "Find failed");

  free(blog->title);
  blog->title = strdup("Updated Title");
  blog->status = 2;

  kfm_error_t err = kfm_update(g_ctx, &blog_model, blog);
  ASSERT(err == KFM_OK, "Update failed");

  kfm_free(&blog_model, blog);

  /* Verify */
  blog = kfm_find_by_id(g_ctx, &blog_model, 1);
  ASSERT(strcmp(blog->title, "Updated Title") == 0, "Title not updated");
  ASSERT(blog->status == 2, "Status not updated");

  kfm_free(&blog_model, blog);
  PASS();
}

static void test_find_all(void) {
  TEST("find all");

  /* Insert more data */
  test_blog_t blog2 = {.id = 2,
                       .title = strdup("Second Post"),
                       .url_slug = strdup("second-post"),
                       .content = strdup("More content"),
                       .status = 1,
                       .created_at = 1700000001};
  kfm_insert(g_ctx, &blog_model, &blog2);
  free(blog2.title);
  free(blog2.url_slug);
  free(blog2.content);

  test_blog_t blog3 = {.id = 3,
                       .title = strdup("Draft Post"),
                       .url_slug = strdup("draft-post"),
                       .content = strdup("Work in progress"),
                       .status = 0,
                       .created_at = 1700000002};
  kfm_insert(g_ctx, &blog_model, &blog3);
  free(blog3.title);
  free(blog3.url_slug);
  free(blog3.content);

  kfm_list_t *list = kfm_find_all(g_ctx, &blog_model);
  ASSERT(list != NULL, "Find all failed");
  ASSERT(list->count == 3, "Wrong count");

  kfm_list_free(list);
  PASS();
}

static void test_find_where(void) {
  TEST("find where");

  kfm_list_t *list = kfm_find_where(g_ctx, &blog_model, "status = 1");
  ASSERT(list != NULL, "Find where failed");
  ASSERT(list->count == 1, "Wrong count with WHERE");

  test_blog_t *blog = kfm_list_get(list, 0);
  ASSERT(strcmp(blog->url_slug, "second-post") == 0, "Wrong blog in result");

  kfm_list_free(list);
  PASS();
}

static void test_find_one(void) {
  TEST("find one");

  test_blog_t *blog =
      kfm_find_one(g_ctx, &blog_model, "url_slug = 'draft-post'");
  ASSERT(blog != NULL, "Find one failed");
  ASSERT(blog->id == 3, "Wrong blog");
  ASSERT(blog->status == 0, "Wrong status");

  kfm_free(&blog_model, blog);
  PASS();
}

static void test_delete(void) {
  TEST("delete");

  test_blog_t *blog = kfm_find_by_id(g_ctx, &blog_model, 3);
  ASSERT(blog != NULL, "Find for delete failed");

  kfm_error_t err = kfm_delete(g_ctx, &blog_model, blog);
  ASSERT(err == KFM_OK, "Delete failed");

  kfm_free(&blog_model, blog);

  /* Verify */
  int count = kfm_count(g_ctx, &blog_model, NULL);
  ASSERT(count == 2, "Count wrong after delete");

  blog = kfm_find_by_id(g_ctx, &blog_model, 3);
  ASSERT(blog == NULL, "Deleted blog still exists");

  PASS();
}

/* ========== JSON conversion tests ========== */

static void test_to_json(void) {
  TEST("to JSON");

  test_blog_t *blog = kfm_find_by_id(g_ctx, &blog_model, 1);
  ASSERT(blog != NULL, "Find failed");

  json_value_t *json = kfm_to_json(&blog_model, blog);
  ASSERT(json != NULL, "To JSON failed");
  ASSERT(json_is_object(json), "Not object");
  ASSERT(json_get_int(json_object_get(json, "id")) == 1, "Wrong id");
  ASSERT(strcmp(json_get_string(json_object_get(json, "title")),
                "Updated Title") == 0,
         "Wrong title");

  char *str = json_stringify(json, false);
  ASSERT(str != NULL, "Stringify failed");
  ASSERT(strstr(str, "\"id\":1") != NULL, "ID not in JSON");

  free(str);
  json_free(json);
  kfm_free(&blog_model, blog);
  PASS();
}

static void test_list_to_json(void) {
  TEST("list to JSON");

  kfm_list_t *list = kfm_find_all(g_ctx, &blog_model);
  ASSERT(list != NULL, "Find all failed");

  json_value_t *json = kfm_list_to_json(list);
  ASSERT(json != NULL, "List to JSON failed");
  ASSERT(json_is_array(json), "Not array");
  ASSERT(json_array_len(json) == 2, "Wrong array length");

  char *str = json_stringify(json, false);
  ASSERT(strstr(str, "first-post") != NULL ||
             strstr(str, "second-post") != NULL,
         "Slugs not in JSON");

  free(str);
  json_free(json);
  kfm_list_free(list);
  PASS();
}

static void test_from_json(void) {
  TEST("from JSON");

  json_value_t *json = json_object_new();
  json_object_set(json, "id", json_int(10));
  json_object_set(json, "title", json_string("From JSON"));
  json_object_set(json, "url_slug", json_string("from-json"));
  json_object_set(json, "content", json_string("Created from JSON"));
  json_object_set(json, "status", json_int(1));

  test_blog_t *blog = kfm_new(&blog_model);
  ASSERT(blog != NULL, "New failed");

  kfm_error_t err = kfm_from_json(&blog_model, blog, json);
  ASSERT(err == KFM_OK, "From JSON failed");
  ASSERT(blog->id == 10, "Wrong id");
  ASSERT(strcmp(blog->title, "From JSON") == 0, "Wrong title");
  ASSERT(strcmp(blog->url_slug, "from-json") == 0, "Wrong slug");

  json_free(json);
  kfm_free(&blog_model, blog);
  PASS();
}

/* ========== Transaction tests ========== */

static void test_transaction_commit(void) {
  TEST("transaction commit");

  kfm_error_t err = kfm_begin(g_ctx);
  ASSERT(err == KFM_OK, "Begin failed");

  test_blog_t blog = {.id = 100,
                      .title = strdup("Transaction Post"),
                      .url_slug = strdup("txn-post"),
                      .content = strdup("In transaction"),
                      .status = 1,
                      .created_at = 1700000003};
  err = kfm_insert(g_ctx, &blog_model, &blog);
  ASSERT(err == KFM_OK, "Insert in txn failed");

  err = kfm_commit(g_ctx);
  ASSERT(err == KFM_OK, "Commit failed");

  test_blog_t *found = kfm_find_by_id(g_ctx, &blog_model, 100);
  ASSERT(found != NULL, "Committed data not found");

  free(blog.title);
  free(blog.url_slug);
  free(blog.content);
  kfm_free(&blog_model, found);
  PASS();
}

static void test_transaction_rollback(void) {
  TEST("transaction rollback");

  int initial_count = kfm_count(g_ctx, &blog_model, NULL);

  kfm_error_t err = kfm_begin(g_ctx);
  ASSERT(err == KFM_OK, "Begin failed");

  test_blog_t blog = {.id = 200,
                      .title = strdup("Rollback Post"),
                      .url_slug = strdup("rollback-post"),
                      .content = strdup("Will be rolled back"),
                      .status = 1,
                      .created_at = 1700000004};
  kfm_insert(g_ctx, &blog_model, &blog);

  err = kfm_rollback(g_ctx);
  ASSERT(err == KFM_OK, "Rollback failed");

  int final_count = kfm_count(g_ctx, &blog_model, NULL);
  ASSERT(final_count == initial_count, "Data not rolled back");

  test_blog_t *found = kfm_find_by_id(g_ctx, &blog_model, 200);
  ASSERT(found == NULL, "Rolled back data still exists");

  free(blog.title);
  free(blog.url_slug);
  free(blog.content);
  PASS();
}

static void test_large_insert(void) {
  TEST("large insert (>8KB)");

  /* Create large content (3KB) - fits in page */
  size_t size = 3 * 1024;
  char *large_content = malloc(size + 1);
  memset(large_content, 'A', size);
  large_content[size] = '\0';

  test_blog_t blog = {.id = 999,
                      .title = strdup("Large Post"),
                      .url_slug = strdup("large-post"),
                      .content = large_content,
                      .status = 1,
                      .created_at = 1700000005};

  kfm_error_t err = kfm_insert(g_ctx, &blog_model, &blog);
  if (err != KFM_OK) {
    printf("DB Error: %s\n", kdb_error_msg(g_db));
    // Also print internal error code
    printf("DB Error Code: %d\n", kdb_error_code(g_db));
  }
  ASSERT(err == KFM_OK, "Large insert failed");

  /* Verify */
  test_blog_t *found = kfm_find_by_id(g_ctx, &blog_model, 999);
  ASSERT(found != NULL, "Large post not found");
  ASSERT(strlen(found->content) == size, "Content size mismatch");

  kfm_free(&blog_model, found);
  free(blog.title);
  free(blog.url_slug);
  free(blog.content); /* large_content */

  PASS();
}

/* ========== Main ========== */

int main(void) {
  printf("\n==================================\n");
  printf("  ORM Test Suite\n");
  printf("==================================\n\n");

  setup();

  printf("Table Tests:\n");
  test_create_table();

  printf("\nCRUD Tests:\n");
  test_insert();
  test_find_by_id();
  test_update();
  test_find_all();
  test_find_where();
  test_find_one();
  test_delete();

  printf("\nJSON Conversion Tests:\n");
  test_to_json();
  test_list_to_json();
  test_from_json();

  printf("\nTransaction Tests:\n");
  test_transaction_commit();
  test_transaction_rollback();

  printf("\nEdge Case Tests:\n");
  test_large_insert();

  teardown();

  printf("\n==================================\n");
  printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
  printf("==================================\n\n");

  return tests_passed == tests_run ? 0 : 1;
}

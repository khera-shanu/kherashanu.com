/**
 * Kherashanu Database Test Suite
 *
 * Comprehensive tests for the lightweight SQL database.
 * Run with: make test
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/db/btree.h"
#include "../src/db/db.h"
#include "../src/db/sql_parser.h"

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

/* ========== B-Tree Tests ========== */

static void test_btree_insert_find(void) {
  TEST("B-tree insert and find");

  btree_t *tree = btree_create(true);
  ASSERT(tree != NULL, "Failed to create B-tree");

  /* Insert 100 keys */
  for (int i = 0; i < 100; i++) {
    btree_key_t key = btree_key_int(i * 2);
    int result = btree_insert(tree, key, i * 10);
    ASSERT(result == 0, "Insert failed");
  }

  ASSERT(btree_size(tree) == 100, "Size mismatch");

  /* Find all keys */
  for (int i = 0; i < 100; i++) {
    btree_key_t key = btree_key_int(i * 2);
    int64_t value;
    bool found = btree_find(tree, &key, &value);
    ASSERT(found, "Key not found");
    ASSERT(value == i * 10, "Value mismatch");
  }

  /* Try to find non-existent key */
  btree_key_t missing = btree_key_int(999);
  int64_t dummy;
  ASSERT(!btree_find(tree, &missing, &dummy), "Found non-existent key");

  btree_destroy(tree);
  PASS();
}

static void test_btree_delete(void) {
  TEST("B-tree delete");

  btree_t *tree = btree_create(true);

  /* Insert keys */
  for (int i = 0; i < 50; i++) {
    btree_key_t key = btree_key_int(i);
    btree_insert(tree, key, i);
  }

  /* Delete every other key */
  for (int i = 0; i < 50; i += 2) {
    btree_key_t key = btree_key_int(i);
    bool deleted = btree_delete(tree, &key);
    ASSERT(deleted, "Delete failed");
  }

  ASSERT(btree_size(tree) == 25, "Size mismatch after delete");

  /* Verify remaining keys */
  for (int i = 1; i < 50; i += 2) {
    btree_key_t key = btree_key_int(i);
    int64_t value;
    ASSERT(btree_find(tree, &key, &value), "Remaining key not found");
  }

  btree_destroy(tree);
  PASS();
}

static void test_btree_string_keys(void) {
  TEST("B-tree string keys");

  btree_t *tree = btree_create(true);

  const char *words[] = {"apple", "banana", "cherry", "date", "elderberry"};
  for (int i = 0; i < 5; i++) {
    btree_key_t key = btree_key_str(words[i]);
    btree_insert(tree, key, i + 1);
    btree_key_free(&key);
  }

  ASSERT(btree_size(tree) == 5, "Size mismatch");

  btree_key_t search = btree_key_str("cherry");
  int64_t value;
  ASSERT(btree_find(tree, &search, &value), "String key not found");
  ASSERT(value == 3, "Value mismatch");
  btree_key_free(&search);

  btree_destroy(tree);
  PASS();
}

static void test_btree_iterator(void) {
  TEST("B-tree iterator");

  btree_t *tree = btree_create(true);

  for (int i = 0; i < 20; i++) {
    btree_key_t key = btree_key_int(i);
    btree_insert(tree, key, i);
  }

  btree_iter_t *iter = btree_iter_create(tree, NULL);
  ASSERT(iter != NULL, "Failed to create iterator");

  int count = 0;
  btree_key_t key;
  int64_t value;
  while (btree_iter_next(iter, &key, &value)) {
    ASSERT(value == count, "Iterator value mismatch");
    btree_key_free(&key);
    count++;
  }

  ASSERT(count == 20, "Iterator count mismatch");

  btree_iter_destroy(iter);
  btree_destroy(tree);
  PASS();
}

/* ========== SQL Parser Tests ========== */

static void test_parser_select(void) {
  TEST("SQL parser SELECT");

  kdb_stmt_t stmt;
  char err[256];

  kdb_error_t err_code = sql_parse(
      "SELECT id, title FROM blogs WHERE status = 1", &stmt, err, sizeof(err));

  ASSERT(err_code == KDB_OK, err);
  ASSERT(stmt.type == STMT_SELECT, "Wrong statement type");
  ASSERT(stmt.stmt.select.num_columns == 2, "Wrong column count");
  ASSERT(strcmp(stmt.stmt.select.columns[0], "id") == 0, "Wrong column name");
  ASSERT(strcmp(stmt.stmt.select.table, "blogs") == 0, "Wrong table name");
  ASSERT(stmt.stmt.select.where != NULL, "WHERE clause missing");

  sql_stmt_free(&stmt);

  /* Test SELECT * */
  err_code = sql_parse("SELECT * FROM users", &stmt, err, sizeof(err));
  ASSERT(err_code == KDB_OK, err);
  ASSERT(stmt.stmt.select.columns == NULL, "Expected NULL columns for *");
  sql_stmt_free(&stmt);

  PASS();
}

static void test_parser_insert(void) {
  TEST("SQL parser INSERT");

  kdb_stmt_t stmt;
  char err[256];

  kdb_error_t err_code =
      sql_parse("INSERT INTO blogs (title, content) VALUES ('Hello', 'World')",
                &stmt, err, sizeof(err));

  ASSERT(err_code == KDB_OK, err);
  ASSERT(stmt.type == STMT_INSERT, "Wrong statement type");
  ASSERT(strcmp(stmt.stmt.insert.table, "blogs") == 0, "Wrong table");
  ASSERT(stmt.stmt.insert.num_columns == 2, "Wrong column count");
  ASSERT(stmt.stmt.insert.num_values == 2, "Wrong value count");
  ASSERT(stmt.stmt.insert.values[0].type == KDB_TYPE_TEXT, "Wrong value type");

  sql_stmt_free(&stmt);
  PASS();
}

static void test_parser_update(void) {
  TEST("SQL parser UPDATE");

  kdb_stmt_t stmt;
  char err[256];

  kdb_error_t err_code =
      sql_parse("UPDATE blogs SET title = 'New Title', status = 1 WHERE id = 5",
                &stmt, err, sizeof(err));

  ASSERT(err_code == KDB_OK, err);
  ASSERT(stmt.type == STMT_UPDATE, "Wrong statement type");
  ASSERT(stmt.stmt.update.num_assignments == 2, "Wrong assignment count");
  ASSERT(stmt.stmt.update.where != NULL, "WHERE missing");

  sql_stmt_free(&stmt);
  PASS();
}

static void test_parser_create_table(void) {
  TEST("SQL parser CREATE TABLE");

  kdb_stmt_t stmt;
  char err[256];

  kdb_error_t err_code = sql_parse("CREATE TABLE blogs (id INTEGER PRIMARY "
                                   "KEY, title TEXT NOT NULL, views INTEGER)",
                                   &stmt, err, sizeof(err));

  ASSERT(err_code == KDB_OK, err);
  ASSERT(stmt.type == STMT_CREATE_TABLE, "Wrong statement type");
  ASSERT(stmt.stmt.create_table.num_columns == 3, "Wrong column count");
  ASSERT(stmt.stmt.create_table.columns[0].primary_key, "Primary key not set");
  ASSERT(stmt.stmt.create_table.columns[1].not_null, "NOT NULL not set");

  sql_stmt_free(&stmt);
  PASS();
}

/* ========== Database CRUD Tests ========== */

static void test_db_create_and_insert(void) {
  TEST("Database CREATE TABLE and INSERT");

  /* Remove test database if exists */
  unlink("/tmp/test_kdb.db");
  unlink("/tmp/test_kdb.db.wal");

  kdb_t *db;
  kdb_error_t err = kdb_open("/tmp/test_kdb.db", &db);
  ASSERT(err == KDB_OK, "Failed to open database");

  /* Create table */
  err = kdb_execute(db, "CREATE TABLE blogs (id INTEGER PRIMARY KEY, title "
                        "TEXT, status INTEGER)");
  ASSERT(err == KDB_OK, kdb_error_msg(db));

  /* Insert rows */
  err = kdb_execute(
      db, "INSERT INTO blogs (id, title, status) VALUES (1, 'First Post', 1)");
  ASSERT(err == KDB_OK, kdb_error_msg(db));

  err = kdb_execute(
      db, "INSERT INTO blogs (id, title, status) VALUES (2, 'Second Post', 0)");
  ASSERT(err == KDB_OK, kdb_error_msg(db));

  err = kdb_execute(
      db, "INSERT INTO blogs (id, title, status) VALUES (3, 'Third Post', 1)");
  ASSERT(err == KDB_OK, kdb_error_msg(db));

  kdb_close(db);
  PASS();
}

static void test_db_select(void) {
  TEST("Database SELECT");

  kdb_t *db;
  kdb_error_t err = kdb_open("/tmp/test_kdb.db", &db);
  ASSERT(err == KDB_OK, "Failed to open database");

  /* Select all */
  kdb_result_t *result;
  err = kdb_query(db, "SELECT * FROM blogs", &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 3, "Wrong row count");
  ASSERT(result->num_columns == 3, "Wrong column count");

  kdb_result_free(result);

  /* Select with WHERE */
  err = kdb_query(db, "SELECT title FROM blogs WHERE status = 1", &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 2, "Wrong row count with WHERE");

  kdb_result_free(result);
  kdb_close(db);
  PASS();
}

static void test_db_update(void) {
  TEST("Database UPDATE");

  kdb_t *db;
  kdb_open("/tmp/test_kdb.db", &db);

  kdb_error_t err =
      kdb_execute(db, "UPDATE blogs SET title = 'Updated Post' WHERE id = 2");
  ASSERT(err == KDB_OK, kdb_error_msg(db));

  /* Verify update */
  kdb_result_t *result;
  err = kdb_query(db, "SELECT title FROM blogs WHERE id = 2", &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 1, "Row not found after update");
  ASSERT(strcmp(result->rows[0].values[0].v.text.data, "Updated Post") == 0,
         "Update not applied");

  kdb_result_free(result);
  kdb_close(db);
  PASS();
}

static void test_db_delete(void) {
  TEST("Database DELETE");

  kdb_t *db;
  kdb_open("/tmp/test_kdb.db", &db);

  kdb_error_t err = kdb_execute(db, "DELETE FROM blogs WHERE status = 0");
  ASSERT(err == KDB_OK, kdb_error_msg(db));

  /* Verify deletion */
  kdb_result_t *result;
  err = kdb_query(db, "SELECT * FROM blogs", &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 2, "Wrong row count after delete");

  kdb_result_free(result);
  kdb_close(db);
  PASS();
}

/* ========== Transaction Tests ========== */

static void test_transaction_commit(void) {
  TEST("Transaction commit");

  unlink("/tmp/test_txn.db");
  unlink("/tmp/test_txn.db.wal");

  kdb_t *db;
  kdb_open("/tmp/test_txn.db", &db);

  kdb_execute(db, "CREATE TABLE test (id INTEGER, value TEXT)");

  /* Begin transaction */
  kdb_error_t err = kdb_txn_begin(db);
  ASSERT(err == KDB_OK, "Begin failed");

  /* Insert within transaction */
  err = kdb_execute(db, "INSERT INTO test (id, value) VALUES (1, 'txn test')");
  ASSERT(err == KDB_OK, kdb_error_msg(db));

  /* Commit */
  err = kdb_txn_commit(db);
  ASSERT(err == KDB_OK, "Commit failed");

  /* Verify data persists */
  kdb_result_t *result;
  err = kdb_query(db, "SELECT * FROM test", &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 1, "Data not persisted after commit");

  kdb_result_free(result);
  kdb_close(db);
  PASS();
}

static void test_transaction_rollback(void) {
  TEST("Transaction rollback");

  kdb_t *db;
  kdb_open("/tmp/test_txn.db", &db);

  /* Get initial count */
  kdb_result_t *result;
  kdb_query(db, "SELECT * FROM test", &result);
  int initial_count = result->num_rows;
  kdb_result_free(result);

  /* Begin transaction */
  kdb_txn_begin(db);

  /* Insert within transaction */
  kdb_execute(db, "INSERT INTO test (id, value) VALUES (2, 'will rollback')");

  /* Rollback */
  kdb_error_t err = kdb_txn_rollback(db);
  ASSERT(err == KDB_OK, "Rollback failed");

  /* Verify data was rolled back */
  kdb_query(db, "SELECT * FROM test", &result);
  ASSERT(result->num_rows == initial_count, "Data not rolled back");

  kdb_result_free(result);
  kdb_close(db);
  PASS();
}

/* ========== Persistence Tests ========== */

static void test_persistence(void) {
  TEST("Database persistence");

  unlink("/tmp/test_persist.db");
  unlink("/tmp/test_persist.db.wal");

  /* Create and populate database */
  kdb_t *db;
  kdb_open("/tmp/test_persist.db", &db);
  kdb_execute(db, "CREATE TABLE persist_test (id INTEGER, name TEXT)");
  kdb_execute(db,
              "INSERT INTO persist_test (id, name) VALUES (1, 'Persistent')");
  kdb_execute(db, "INSERT INTO persist_test (id, name) VALUES (2, 'Data')");
  kdb_close(db);

  /* Reopen and verify */
  kdb_open("/tmp/test_persist.db", &db);

  kdb_result_t *result;
  kdb_error_t err = kdb_query(db, "SELECT * FROM persist_test", &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 2, "Data not persisted");
  ASSERT(strcmp(result->rows[0].values[1].v.text.data, "Persistent") == 0,
         "Data corrupted");

  kdb_result_free(result);
  kdb_close(db);
  PASS();
}

/* ========== Complex Query Tests ========== */

static void test_complex_where(void) {
  TEST("Complex WHERE clauses");

  unlink("/tmp/test_where.db");
  unlink("/tmp/test_where.db.wal");

  kdb_t *db;
  kdb_open("/tmp/test_where.db", &db);

  kdb_execute(db, "CREATE TABLE items (id INTEGER, category TEXT, price REAL)");
  kdb_execute(
      db, "INSERT INTO items (id, category, price) VALUES (1, 'books', 19.99)");
  kdb_execute(db, "INSERT INTO items (id, category, price) VALUES (2, "
                  "'electronics', 299.99)");
  kdb_execute(
      db, "INSERT INTO items (id, category, price) VALUES (3, 'books', 9.99)");
  kdb_execute(db, "INSERT INTO items (id, category, price) VALUES (4, "
                  "'electronics', 49.99)");

  /* Test AND */
  kdb_result_t *result;
  kdb_error_t err = kdb_query(
      db, "SELECT * FROM items WHERE category = 'books' AND price < 15.0",
      &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 1, "AND query wrong count");
  kdb_result_free(result);

  /* Test OR */
  err = kdb_query(
      db, "SELECT * FROM items WHERE price > 100.0 OR category = 'books'",
      &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 3, "OR query wrong count");
  kdb_result_free(result);

  /* Test LIKE */
  err =
      kdb_query(db, "SELECT * FROM items WHERE category LIKE 'elec%'", &result);
  ASSERT(err == KDB_OK, kdb_error_msg(db));
  ASSERT(result->num_rows == 2, "LIKE query wrong count");
  kdb_result_free(result);

  kdb_close(db);
  PASS();
}

/* ========== Main ========== */

int main(void) {
  printf("\n==================================\n");
  printf("  Kherashanu Database Test Suite\n");
  printf("==================================\n\n");

  printf("B-Tree Tests:\n");
  test_btree_insert_find();
  test_btree_delete();
  test_btree_string_keys();
  test_btree_iterator();

  printf("\nSQL Parser Tests:\n");
  test_parser_select();
  test_parser_insert();
  test_parser_update();
  test_parser_create_table();

  printf("\nDatabase CRUD Tests:\n");
  test_db_create_and_insert();
  test_db_select();
  test_db_update();
  test_db_delete();

  printf("\nTransaction Tests:\n");
  test_transaction_commit();
  test_transaction_rollback();

  printf("\nPersistence Tests:\n");
  test_persistence();

  printf("\nComplex Query Tests:\n");
  test_complex_where();

  printf("\n==================================\n");
  printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
  printf("==================================\n\n");

  /* Cleanup */
  unlink("/tmp/test_kdb.db");
  unlink("/tmp/test_kdb.db.wal");
  unlink("/tmp/test_txn.db");
  unlink("/tmp/test_txn.db.wal");
  unlink("/tmp/test_persist.db");
  unlink("/tmp/test_persist.db.wal");
  unlink("/tmp/test_where.db");
  unlink("/tmp/test_where.db.wal");

  return tests_passed == tests_run ? 0 : 1;
}

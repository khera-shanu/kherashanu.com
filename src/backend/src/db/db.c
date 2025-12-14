/**
 * Kherashanu Database - Main Interface Implementation
 */
#include "db.h"
#include "query.h"
#include "sql_parser.h"
#include "storage.h"
#include "transaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Database handle */
struct kdb_s {
  storage_t *store;
  txn_manager_t txn_mgr;
  query_ctx_t query_ctx;
  char error_msg[256];
  kdb_error_t error_code;
  char path[512];
};

/*
 * Open or create database
 */
kdb_error_t kdb_open(const char *path, kdb_t **db) {
  if (!path || !db)
    return KDB_ERR_IO;

  kdb_t *d = calloc(1, sizeof(kdb_t));
  if (!d)
    return KDB_ERR_NOMEM;

  strncpy(d->path, path, sizeof(d->path) - 1);

  /* Open storage */
  kdb_error_t err = storage_open(path, &d->store);
  if (err != KDB_OK) {
    free(d);
    return err;
  }

  /* Initialize transaction manager */
  err = txn_manager_init(&d->txn_mgr, d->store, path);
  if (err != KDB_OK) {
    storage_close(d->store);
    free(d);
    return err;
  }

  /* Recover from WAL if needed */
  txn_recover(&d->txn_mgr);

  /* Set up query context */
  d->query_ctx.store = d->store;
  d->query_ctx.txn_mgr = &d->txn_mgr;

  *db = d;
  return KDB_OK;
}

/*
 * Close database
 */
void kdb_close(kdb_t *db) {
  if (!db)
    return;

  txn_manager_cleanup(&db->txn_mgr);
  storage_close(db->store);
  free(db);
}

/*
 * Execute SQL (INSERT, UPDATE, DELETE, CREATE, DROP)
 */
kdb_error_t kdb_execute(kdb_t *db, const char *sql) {
  if (!db || !sql)
    return KDB_ERR_IO;

  kdb_stmt_t stmt = {0};
  char err_buf[256];

  kdb_error_t err = sql_parse(sql, &stmt, err_buf, sizeof(err_buf));
  if (err != KDB_OK) {
    strncpy(db->error_msg, err_buf, sizeof(db->error_msg) - 1);
    db->error_code = err;
    return err;
  }

  switch (stmt.type) {
  case STMT_SELECT:
    snprintf(db->error_msg, sizeof(db->error_msg),
             "Use kdb_query() for SELECT statements");
    sql_stmt_free(&stmt);
    return KDB_ERR_SYNTAX;

  case STMT_INSERT: {
    int64_t last_id;
    err = query_exec_insert(&db->query_ctx, &stmt.stmt.insert, &last_id);
    break;
  }

  case STMT_UPDATE: {
    int affected;
    err = query_exec_update(&db->query_ctx, &stmt.stmt.update, &affected);
    break;
  }

  case STMT_DELETE: {
    int affected;
    err = query_exec_delete(&db->query_ctx, &stmt.stmt.del, &affected);
    break;
  }

  case STMT_CREATE_TABLE:
    err = query_exec_create_table(&db->query_ctx, &stmt.stmt.create_table);
    break;

  case STMT_DROP_TABLE:
    err = query_exec_drop_table(&db->query_ctx, &stmt.stmt.drop_table);
    break;
  }

  if (err != KDB_OK) {
    strncpy(db->error_msg, db->query_ctx.error_msg, sizeof(db->error_msg) - 1);
  }

  sql_stmt_free(&stmt);
  db->error_code = err;
  return err;
}

/*
 * Execute SELECT query
 */
kdb_error_t kdb_query(kdb_t *db, const char *sql, kdb_result_t **result) {
  if (!db || !sql || !result)
    return KDB_ERR_IO;

  kdb_stmt_t stmt = {0};
  char err_buf[256];

  kdb_error_t err = sql_parse(sql, &stmt, err_buf, sizeof(err_buf));
  if (err != KDB_OK) {
    strncpy(db->error_msg, err_buf, sizeof(db->error_msg) - 1);
    db->error_code = err;
    return err;
  }

  if (stmt.type != STMT_SELECT) {
    snprintf(db->error_msg, sizeof(db->error_msg),
             "Use kdb_execute() for non-SELECT statements");
    sql_stmt_free(&stmt);
    return KDB_ERR_SYNTAX;
  }

  err = query_exec_select(&db->query_ctx, &stmt.stmt.select, result);

  if (err != KDB_OK) {
    strncpy(db->error_msg, db->query_ctx.error_msg, sizeof(db->error_msg) - 1);
  }

  sql_stmt_free(&stmt);
  db->error_code = err;
  return err;
}

/*
 * Free result set
 */
void kdb_result_free(kdb_result_t *result) {
  if (!result)
    return;

  for (int i = 0; i < result->num_columns; i++) {
    free(result->columns[i]);
  }
  free(result->columns);

  for (int i = 0; i < result->num_rows; i++) {
    for (int j = 0; j < result->rows[i].num_values; j++) {
      kdb_value_free(&result->rows[i].values[j]);
    }
    free(result->rows[i].values);
  }
  free(result->rows);
  free(result);
}

/*
 * Get error message
 */
const char *kdb_error_msg(kdb_t *db) {
  return db ? db->error_msg : "Invalid database handle";
}

/*
 * Get error code
 */
kdb_error_t kdb_error_code(kdb_t *db) {
  return db ? db->error_code : KDB_ERR_IO;
}

/*
 * Transaction functions
 */
kdb_error_t kdb_txn_begin(kdb_t *db) {
  if (!db)
    return KDB_ERR_IO;
  return txn_begin(&db->txn_mgr);
}

kdb_error_t kdb_txn_commit(kdb_t *db) {
  if (!db)
    return KDB_ERR_IO;
  return txn_commit(&db->txn_mgr);
}

kdb_error_t kdb_txn_rollback(kdb_t *db) {
  if (!db)
    return KDB_ERR_IO;
  return txn_rollback(&db->txn_mgr);
}

/*
 * Value utility functions
 */
void kdb_value_free(kdb_value_t *val) {
  if (!val)
    return;

  switch (val->type) {
  case KDB_TYPE_TEXT:
    free(val->v.text.data);
    val->v.text.data = NULL;
    val->v.text.len = 0;
    break;
  case KDB_TYPE_BLOB:
    free(val->v.blob.data);
    val->v.blob.data = NULL;
    val->v.blob.len = 0;
    break;
  default:
    break;
  }
  val->type = KDB_TYPE_NULL;
}

kdb_value_t kdb_value_int(int64_t i) {
  kdb_value_t val = {0};
  val.type = KDB_TYPE_INTEGER;
  val.v.i = i;
  return val;
}

kdb_value_t kdb_value_real(double r) {
  kdb_value_t val = {0};
  val.type = KDB_TYPE_REAL;
  val.v.r = r;
  return val;
}

kdb_value_t kdb_value_text(const char *text) {
  kdb_value_t val = {0};
  val.type = KDB_TYPE_TEXT;
  if (text) {
    size_t len = strlen(text);
    val.v.text.data = malloc(len + 1);
    if (val.v.text.data) {
      memcpy(val.v.text.data, text, len + 1);
      val.v.text.len = len;
    }
  }
  return val;
}

kdb_value_t kdb_value_null(void) {
  kdb_value_t val = {0};
  val.type = KDB_TYPE_NULL;
  return val;
}

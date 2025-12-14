/**
 * Query Executor - Executes parsed SQL statements
 */
#ifndef KDB_QUERY_H
#define KDB_QUERY_H

#include "db.h"
#include "sql_parser.h"
#include "storage.h"
#include "transaction.h"

/* Query execution context */
typedef struct {
  storage_t *store;
  txn_manager_t *txn_mgr;
  char error_msg[256];
  kdb_error_t error_code;
} query_ctx_t;

/**
 * Execute SELECT query
 */
kdb_error_t query_exec_select(query_ctx_t *ctx, const select_stmt_t *stmt,
                              kdb_result_t **result);

/**
 * Execute INSERT statement
 */
kdb_error_t query_exec_insert(query_ctx_t *ctx, const insert_stmt_t *stmt,
                              int64_t *last_id);

/**
 * Execute UPDATE statement
 */
kdb_error_t query_exec_update(query_ctx_t *ctx, const update_stmt_t *stmt,
                              int *rows_affected);

/**
 * Execute DELETE statement
 */
kdb_error_t query_exec_delete(query_ctx_t *ctx, const delete_stmt_t *stmt,
                              int *rows_affected);

/**
 * Execute CREATE TABLE
 */
kdb_error_t query_exec_create_table(query_ctx_t *ctx,
                                    const create_table_stmt_t *stmt);

/**
 * Execute DROP TABLE
 */
kdb_error_t query_exec_drop_table(query_ctx_t *ctx,
                                  const drop_table_stmt_t *stmt);

/**
 * Evaluate expression against a row
 * Returns true if expression evaluates to true
 */
bool query_eval_expr(const expr_t *expr, const table_t *table,
                     const kdb_row_t *row);

#endif /* KDB_QUERY_H */

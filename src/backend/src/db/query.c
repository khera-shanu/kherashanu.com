/**
 * Query Executor Implementation
 */
#include "query.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/*
 * Get column index by name
 */
static int get_column_index(const table_t *table, const char *name) {
  for (int i = 0; i < table->num_columns; i++) {
    if (strcasecmp(table->columns[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

/*
 * Compare two values
 */
static int compare_values(const kdb_value_t *a, const kdb_value_t *b) {
  if (a->type != b->type) {
    return (int)a->type - (int)b->type;
  }

  switch (a->type) {
  case KDB_TYPE_NULL:
    return 0;
  case KDB_TYPE_INTEGER:
  case KDB_TYPE_TIMESTAMP:
    if (a->v.i < b->v.i)
      return -1;
    if (a->v.i > b->v.i)
      return 1;
    return 0;
  case KDB_TYPE_REAL:
    if (a->v.r < b->v.r)
      return -1;
    if (a->v.r > b->v.r)
      return 1;
    return 0;
  case KDB_TYPE_TEXT:
    return strcmp(a->v.text.data ? a->v.text.data : "",
                  b->v.text.data ? b->v.text.data : "");
  default:
    return 0;
  }
}

/*
 * LIKE pattern matching
 */
static bool like_match(const char *pattern, const char *str) {
  if (!pattern || !str)
    return false;

  while (*pattern && *str) {
    if (*pattern == '%') {
      pattern++;
      if (!*pattern)
        return true;
      while (*str) {
        if (like_match(pattern, str))
          return true;
        str++;
      }
      return false;
    } else if (*pattern == '_') {
      pattern++;
      str++;
    } else if (tolower((unsigned char)*pattern) ==
               tolower((unsigned char)*str)) {
      pattern++;
      str++;
    } else {
      return false;
    }
  }

  while (*pattern == '%')
    pattern++;
  return !*pattern && !*str;
}

/*
 * Evaluate expression against a row
 */
bool query_eval_expr(const expr_t *expr, const table_t *table,
                     const kdb_row_t *row) {
  if (!expr)
    return true;

  switch (expr->type) {
  case EXPR_VALUE:
    /* A value alone is truthy if non-null and non-zero */
    if (expr->v.value.type == KDB_TYPE_NULL)
      return false;
    if (expr->v.value.type == KDB_TYPE_INTEGER)
      return expr->v.value.v.i != 0;
    return true;

  case EXPR_COLUMN: {
    int idx = get_column_index(table, expr->v.column);
    if (idx < 0 || idx >= row->num_values)
      return false;
    return row->values[idx].type != KDB_TYPE_NULL;
  }

  case EXPR_COMPARE: {
    /* Get left value */
    kdb_value_t left_val = {0};
    if (expr->v.compare.left->type == EXPR_COLUMN) {
      int idx = get_column_index(table, expr->v.compare.left->v.column);
      if (idx >= 0 && idx < row->num_values) {
        left_val = row->values[idx];
      }
    } else if (expr->v.compare.left->type == EXPR_VALUE) {
      left_val = expr->v.compare.left->v.value;
    }

    /* Handle IS NULL / IS NOT NULL */
    if (expr->v.compare.op == OP_IS_NULL) {
      return left_val.type == KDB_TYPE_NULL;
    }
    if (expr->v.compare.op == OP_IS_NOT_NULL) {
      return left_val.type != KDB_TYPE_NULL;
    }

    /* Get right value */
    kdb_value_t right_val = {0};
    if (expr->v.compare.right) {
      if (expr->v.compare.right->type == EXPR_COLUMN) {
        int idx = get_column_index(table, expr->v.compare.right->v.column);
        if (idx >= 0 && idx < row->num_values) {
          right_val = row->values[idx];
        }
      } else if (expr->v.compare.right->type == EXPR_VALUE) {
        right_val = expr->v.compare.right->v.value;
      }
    }

    /* Handle LIKE */
    if (expr->v.compare.op == OP_LIKE) {
      if (left_val.type != KDB_TYPE_TEXT || right_val.type != KDB_TYPE_TEXT) {
        return false;
      }
      return like_match(right_val.v.text.data, left_val.v.text.data);
    }

    /* Compare */
    int cmp = compare_values(&left_val, &right_val);

    switch (expr->v.compare.op) {
    case OP_EQ:
      return cmp == 0;
    case OP_NE:
      return cmp != 0;
    case OP_LT:
      return cmp < 0;
    case OP_LE:
      return cmp <= 0;
    case OP_GT:
      return cmp > 0;
    case OP_GE:
      return cmp >= 0;
    default:
      return false;
    }
  }

  case EXPR_LOGIC: {
    bool left = query_eval_expr(expr->v.logic.left, table, row);

    if (expr->v.logic.op == LOGIC_AND) {
      if (!left)
        return false;
      return query_eval_expr(expr->v.logic.right, table, row);
    } else { /* LOGIC_OR */
      if (left)
        return true;
      return query_eval_expr(expr->v.logic.right, table, row);
    }
  }
  }

  return false;
}

/*
 * Copy a value (deep copy for strings/blobs)
 */
static void copy_value(kdb_value_t *dst, const kdb_value_t *src) {
  dst->type = src->type;
  switch (src->type) {
  case KDB_TYPE_INTEGER:
  case KDB_TYPE_TIMESTAMP:
    dst->v.i = src->v.i;
    break;
  case KDB_TYPE_REAL:
    dst->v.r = src->v.r;
    break;
  case KDB_TYPE_TEXT:
    if (src->v.text.data) {
      dst->v.text.data = malloc(src->v.text.len + 1);
      memcpy(dst->v.text.data, src->v.text.data, src->v.text.len + 1);
      dst->v.text.len = src->v.text.len;
    }
    break;
  case KDB_TYPE_BLOB:
    if (src->v.blob.data) {
      dst->v.blob.data = malloc(src->v.blob.len);
      memcpy(dst->v.blob.data, src->v.blob.data, src->v.blob.len);
      dst->v.blob.len = src->v.blob.len;
    }
    break;
  default:
    break;
  }
}

/*
 * Execute SELECT
 */
kdb_error_t query_exec_select(query_ctx_t *ctx, const select_stmt_t *stmt,
                              kdb_result_t **result) {
  if (!ctx || !stmt || !result)
    return KDB_ERR_IO;

  table_t *table = storage_get_table(ctx->store, stmt->table);
  if (!table) {
    snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Table not found: %s",
             stmt->table);
    return KDB_ERR_NO_TABLE;
  }

  /* Determine columns to return */
  int *col_indices = NULL;
  int num_cols = 0;

  if (stmt->columns == NULL || stmt->num_columns == 0) {
    /* SELECT * */
    num_cols = table->num_columns;
    col_indices = malloc(num_cols * sizeof(int));
    for (int i = 0; i < num_cols; i++) {
      col_indices[i] = i;
    }
  } else {
    num_cols = stmt->num_columns;
    col_indices = malloc(num_cols * sizeof(int));
    for (int i = 0; i < num_cols; i++) {
      col_indices[i] = get_column_index(table, stmt->columns[i]);
      if (col_indices[i] < 0) {
        free(col_indices);
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Column not found: %s",
                 stmt->columns[i]);
        return KDB_ERR_NO_COLUMN;
      }
    }
  }

  /* Allocate result */
  kdb_result_t *res = calloc(1, sizeof(kdb_result_t));
  res->num_columns = num_cols;
  res->columns = malloc(num_cols * sizeof(char *));
  for (int i = 0; i < num_cols; i++) {
    res->columns[i] = strdup(table->columns[col_indices[i]].name);
  }

  /* Collect matching rows */
  int row_cap = 16;
  res->rows = malloc(row_cap * sizeof(kdb_row_t));
  res->num_rows = 0;

  storage_cursor_t *cursor = storage_cursor_create(ctx->store, table);
  int64_t rowid;
  kdb_row_t row;
  int count = 0;

  while (storage_cursor_next(cursor, &rowid, &row)) {
    /* Apply WHERE filter */
    if (stmt->where && !query_eval_expr(stmt->where, table, &row)) {
      /* Free row values */
      for (int i = 0; i < row.num_values; i++) {
        kdb_value_free(&row.values[i]);
      }
      free(row.values);
      continue;
    }

    /* Check limit */
    if (stmt->limit >= 0 && count >= stmt->limit) {
      for (int i = 0; i < row.num_values; i++) {
        kdb_value_free(&row.values[i]);
      }
      free(row.values);
      break;
    }

    /* Add to result */
    if (res->num_rows >= row_cap) {
      row_cap *= 2;
      res->rows = realloc(res->rows, row_cap * sizeof(kdb_row_t));
    }

    kdb_row_t *out_row = &res->rows[res->num_rows++];
    out_row->num_values = num_cols;
    out_row->values = calloc(num_cols, sizeof(kdb_value_t));

    for (int i = 0; i < num_cols; i++) {
      copy_value(&out_row->values[i], &row.values[col_indices[i]]);
    }

    /* Free original row */
    for (int i = 0; i < row.num_values; i++) {
      kdb_value_free(&row.values[i]);
    }
    free(row.values);
    count++;
  }

  storage_cursor_destroy(cursor);
  free(col_indices);

  *result = res;
  return KDB_OK;
}

/*
 * Execute INSERT
 */
kdb_error_t query_exec_insert(query_ctx_t *ctx, const insert_stmt_t *stmt,
                              int64_t *last_id) {
  if (!ctx || !stmt)
    return KDB_ERR_IO;

  table_t *table = storage_get_table(ctx->store, stmt->table);
  if (!table) {
    snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Table not found: %s",
             stmt->table);
    return KDB_ERR_NO_TABLE;
  }

  /* Build values array */
  kdb_value_t *values = calloc(table->num_columns, sizeof(kdb_value_t));

  if (stmt->columns && stmt->num_columns > 0) {
    /* Named columns */
    for (int i = 0; i < stmt->num_columns; i++) {
      int idx = get_column_index(table, stmt->columns[i]);
      if (idx < 0) {
        free(values);
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Column not found: %s",
                 stmt->columns[i]);
        return KDB_ERR_NO_COLUMN;
      }
      copy_value(&values[idx], &stmt->values[i]);
    }
  } else {
    /* Positional values */
    for (int i = 0; i < stmt->num_values && i < table->num_columns; i++) {
      copy_value(&values[i], &stmt->values[i]);
    }
  }

  int64_t rowid;
  kdb_error_t err =
      storage_insert_row(ctx->store, table, values, table->num_columns, &rowid);

  if (err == KDB_OK && ctx->txn_mgr) {
    txn_log_insert(ctx->txn_mgr, stmt->table, rowid);
  }

  /* Free values */
  for (int i = 0; i < table->num_columns; i++) {
    kdb_value_free(&values[i]);
  }
  free(values);

  if (last_id)
    *last_id = rowid;
  return err;
}

/*
 * Execute UPDATE
 */
kdb_error_t query_exec_update(query_ctx_t *ctx, const update_stmt_t *stmt,
                              int *rows_affected) {
  if (!ctx || !stmt)
    return KDB_ERR_IO;

  table_t *table = storage_get_table(ctx->store, stmt->table);
  if (!table) {
    snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Table not found: %s",
             stmt->table);
    return KDB_ERR_NO_TABLE;
  }

  int affected = 0;
  storage_cursor_t *cursor = storage_cursor_create(ctx->store, table);
  int64_t rowid;
  kdb_row_t row;

  /* Collect rows to update (we modify list after iteration) */
  int64_t *rowids_to_update = NULL;
  kdb_row_t *new_values = NULL;
  int update_count = 0;
  int update_cap = 16;
  rowids_to_update = malloc(update_cap * sizeof(int64_t));
  new_values = malloc(update_cap * sizeof(kdb_row_t));

  while (storage_cursor_next(cursor, &rowid, &row)) {
    if (stmt->where && !query_eval_expr(stmt->where, table, &row)) {
      for (int i = 0; i < row.num_values; i++) {
        kdb_value_free(&row.values[i]);
      }
      free(row.values);
      continue;
    }

    /* Apply updates */
    for (int i = 0; i < stmt->num_assignments; i++) {
      int idx = get_column_index(table, stmt->assignments[i].column);
      if (idx >= 0 && idx < row.num_values) {
        kdb_value_free(&row.values[idx]);
        copy_value(&row.values[idx], &stmt->assignments[i].value);
      }
    }

    if (update_count >= update_cap) {
      update_cap *= 2;
      rowids_to_update =
          realloc(rowids_to_update, update_cap * sizeof(int64_t));
      new_values = realloc(new_values, update_cap * sizeof(kdb_row_t));
    }

    rowids_to_update[update_count] = rowid;
    new_values[update_count] = row;
    update_count++;
  }

  storage_cursor_destroy(cursor);

  /* Apply updates (delete old, insert new) */
  for (int i = 0; i < update_count; i++) {
    storage_delete_row(ctx->store, table, rowids_to_update[i]);

    int64_t new_rowid;
    storage_insert_row(ctx->store, table, new_values[i].values,
                       new_values[i].num_values, &new_rowid);

    /* Free row values */
    for (int j = 0; j < new_values[i].num_values; j++) {
      kdb_value_free(&new_values[i].values[j]);
    }
    free(new_values[i].values);
    affected++;
  }

  free(rowids_to_update);
  free(new_values);

  if (rows_affected)
    *rows_affected = affected;
  return KDB_OK;
}

/*
 * Execute DELETE
 */
kdb_error_t query_exec_delete(query_ctx_t *ctx, const delete_stmt_t *stmt,
                              int *rows_affected) {
  if (!ctx || !stmt)
    return KDB_ERR_IO;

  table_t *table = storage_get_table(ctx->store, stmt->table);
  if (!table) {
    snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Table not found: %s",
             stmt->table);
    return KDB_ERR_NO_TABLE;
  }

  int affected = 0;
  storage_cursor_t *cursor = storage_cursor_create(ctx->store, table);
  int64_t rowid;
  kdb_row_t row;

  /* Collect rows to delete */
  int64_t *rowids = NULL;
  int count = 0;
  int cap = 16;
  rowids = malloc(cap * sizeof(int64_t));

  while (storage_cursor_next(cursor, &rowid, &row)) {
    bool should_delete = true;

    if (stmt->where) {
      should_delete = query_eval_expr(stmt->where, table, &row);
    }

    for (int i = 0; i < row.num_values; i++) {
      kdb_value_free(&row.values[i]);
    }
    free(row.values);

    if (should_delete) {
      if (count >= cap) {
        cap *= 2;
        rowids = realloc(rowids, cap * sizeof(int64_t));
      }
      rowids[count++] = rowid;
    }
  }

  storage_cursor_destroy(cursor);

  /* Delete collected rows */
  for (int i = 0; i < count; i++) {
    if (storage_delete_row(ctx->store, table, rowids[i]) == KDB_OK) {
      affected++;
    }
  }

  free(rowids);

  if (rows_affected)
    *rows_affected = affected;
  return KDB_OK;
}

/*
 * Execute CREATE TABLE
 */
kdb_error_t query_exec_create_table(query_ctx_t *ctx,
                                    const create_table_stmt_t *stmt) {
  if (!ctx || !stmt)
    return KDB_ERR_IO;

  return storage_create_table(ctx->store, stmt->table, stmt->columns,
                              stmt->num_columns);
}

/*
 * Execute DROP TABLE
 */
kdb_error_t query_exec_drop_table(query_ctx_t *ctx,
                                  const drop_table_stmt_t *stmt) {
  if (!ctx || !stmt)
    return KDB_ERR_IO;

  kdb_error_t err = storage_drop_table(ctx->store, stmt->table);

  if (err == KDB_ERR_NO_TABLE && stmt->if_exists) {
    return KDB_OK;
  }

  return err;
}

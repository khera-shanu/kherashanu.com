/**
 * Kherashanu ORM - Implementation
 */
#include "orm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== Context management ========== */

/* Dynamic buffer for SQL generation */
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} dyn_buf_t;

static int dbuf_init(dyn_buf_t *buf, size_t initial_cap) {
  buf->data = malloc(initial_cap);
  if (!buf->data)
    return -1;
  buf->len = 0;
  buf->cap = initial_cap;
  buf->data[0] = '\0';
  return 0;
}

static void dbuf_free(dyn_buf_t *buf) {
  free(buf->data);
  buf->data = NULL;
  buf->len = 0;
  buf->cap = 0;
}

static int dbuf_append(dyn_buf_t *buf, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  /* Get required size */
  va_list args_copy;
  va_copy(args_copy, args);
  int needed = vsnprintf(NULL, 0, fmt, args_copy);
  va_end(args_copy);

  if (needed < 0) {
    va_end(args);
    return -1;
  }

  /* Resize if needed */
  if (buf->len + needed + 1 > buf->cap) {
    size_t new_cap = buf->cap * 2;
    if (new_cap < buf->len + needed + 1)
      new_cap = buf->len + needed + 1 + 1024;

    char *new_data = realloc(buf->data, new_cap);
    if (!new_data) {
      va_end(args);
      return -1;
    }
    buf->data = new_data;
    buf->cap = new_cap;
  }

  /* Append */
  vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, args);
  buf->len += needed;

  va_end(args);
  return 0;
}

kfm_ctx_t *kfm_init(kdb_t *db) {
  if (!db)
    return NULL;

  kfm_ctx_t *ctx = calloc(1, sizeof(kfm_ctx_t));
  if (!ctx)
    return NULL;

  ctx->db = db;
  return ctx;
}

void kfm_destroy(kfm_ctx_t *ctx) {
  if (ctx) {
    free(ctx);
  }
}

kdb_t *kfm_get_db(kfm_ctx_t *ctx) { return ctx ? ctx->db : NULL; }

/* ========== SQL generation helpers ========== */

static const char *type_to_sql(kdb_type_t type) {
  switch (type) {
  case KDB_TYPE_INTEGER:
    return "INTEGER";
  case KDB_TYPE_REAL:
    return "REAL";
  case KDB_TYPE_TEXT:
    return "TEXT";
  case KDB_TYPE_BLOB:
    return "BLOB";
  case KDB_TYPE_TIMESTAMP:
    return "INTEGER"; /* Store as Unix timestamp */
  default:
    return "TEXT";
  }
}

/* Escape string for SQL (simple doubling of single quotes) */
static char *escape_string(const char *str) {
  if (!str)
    return strdup("NULL");

  size_t len = strlen(str);
  size_t quotes = 0;
  for (size_t i = 0; i < len; i++) {
    if (str[i] == '\'')
      quotes++;
  }

  char *escaped =
      malloc(len + quotes + 3); /* + 2 for surrounding quotes + 1 for null */
  if (!escaped)
    return NULL;

  char *p = escaped;
  *p++ = '\'';
  for (size_t i = 0; i < len; i++) {
    if (str[i] == '\'') {
      *p++ = '\'';
      *p++ = '\'';
    } else {
      *p++ = str[i];
    }
  }
  *p++ = '\'';
  *p = '\0';

  return escaped;
}

/* Get field value as SQL string */
static char *field_to_sql(const kfm_model_t *model, const kfm_field_t *field,
                          void *obj) {
  char buf[64];
  char *ptr = (char *)obj + field->offset;

  switch (field->type) {
  case KDB_TYPE_INTEGER:
  case KDB_TYPE_TIMESTAMP: {
    int64_t val = *(int64_t *)ptr;
    snprintf(buf, sizeof(buf), "%lld", (long long)val);
    return strdup(buf);
  }

  case KDB_TYPE_REAL: {
    double val = *(double *)ptr;
    snprintf(buf, sizeof(buf), "%g", val);
    return strdup(buf);
  }

  case KDB_TYPE_TEXT: {
    char **str_ptr = (char **)ptr;
    return escape_string(*str_ptr);
  }

  default:
    return strdup("NULL");
  }
}

/* Set field value from database result */
static void set_field_from_value(const kfm_field_t *field, void *obj,
                                 const kdb_value_t *val) {
  char *ptr = (char *)obj + field->offset;

  if (val->type == KDB_TYPE_NULL) {
    /* For strings, set to NULL; for numbers, set to 0 */
    if (field->type == KDB_TYPE_TEXT) {
      *(char **)ptr = NULL;
    } else if (field->type == KDB_TYPE_INTEGER ||
               field->type == KDB_TYPE_TIMESTAMP) {
      *(int64_t *)ptr = 0;
    } else if (field->type == KDB_TYPE_REAL) {
      *(double *)ptr = 0.0;
    }
    return;
  }

  switch (field->type) {
  case KDB_TYPE_INTEGER:
  case KDB_TYPE_TIMESTAMP:
    if (val->type == KDB_TYPE_INTEGER) {
      *(int64_t *)ptr = val->v.i;
    } else if (val->type == KDB_TYPE_REAL) {
      *(int64_t *)ptr = (int64_t)val->v.r;
    }
    break;

  case KDB_TYPE_REAL:
    if (val->type == KDB_TYPE_REAL) {
      *(double *)ptr = val->v.r;
    } else if (val->type == KDB_TYPE_INTEGER) {
      *(double *)ptr = (double)val->v.i;
    }
    break;

  case KDB_TYPE_TEXT:
    if (val->type == KDB_TYPE_TEXT && val->v.text.data) {
      *(char **)ptr = strdup(val->v.text.data);
    } else {
      *(char **)ptr = NULL;
    }
    break;

  default:
    break;
  }
}

/* ========== Table operations ========== */

kfm_error_t kfm_create_table(kfm_ctx_t *ctx, const kfm_model_t *model) {
  if (!ctx || !model)
    return KFM_ERR_INVALID;

  /* Use dynamic buffer */
  dyn_buf_t sql;
  if (dbuf_init(&sql, 1024) < 0)
    return KFM_ERR_NOMEM;

  dbuf_append(&sql, "CREATE TABLE %s (", model->table_name);

  for (int i = 0; i < model->num_fields; i++) {
    const kfm_field_t *f = &model->fields[i];
    if (i > 0)
      dbuf_append(&sql, ", ");

    dbuf_append(&sql, "%s %s", f->name, type_to_sql(f->type));

    if (f->flags & KFM_FLAG_PRIMARY_KEY)
      dbuf_append(&sql, " PRIMARY KEY");
    if (f->flags & KFM_FLAG_NOT_NULL)
      dbuf_append(&sql, " NOT NULL");
    if (f->flags & KFM_FLAG_UNIQUE)
      dbuf_append(&sql, " UNIQUE");
  }

  dbuf_append(&sql, ")");

  kdb_error_t err = kdb_execute(ctx->db, sql.data);
  dbuf_free(&sql);
  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

kfm_error_t kfm_drop_table(kfm_ctx_t *ctx, const kfm_model_t *model) {
  if (!ctx || !model)
    return KFM_ERR_INVALID;

  char sql[256];
  snprintf(sql, sizeof(sql), "DROP TABLE %s", model->table_name);

  kdb_error_t err = kdb_execute(ctx->db, sql);
  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

/* ========== CRUD operations ========== */

kfm_error_t kfm_insert(kfm_ctx_t *ctx, const kfm_model_t *model, void *obj) {
  if (!ctx || !model || !obj)
    return KFM_ERR_INVALID;

  /* Use dynamic buffer */
  dyn_buf_t sql;
  if (dbuf_init(&sql, 1024) < 0)
    return KFM_ERR_NOMEM;

  int res = dbuf_append(&sql, "INSERT INTO %s (", model->table_name);
  if (res < 0) {
    dbuf_free(&sql);
    return KFM_ERR_NOMEM;
  }

  /* Column names (skip auto-increment primary key with value 0) */
  int first = 1;
  for (int i = 0; i < model->num_fields; i++) {
    const kfm_field_t *f = &model->fields[i];

    /* Skip auto-increment PK if value is 0 */
    if ((f->flags & KFM_FLAG_PRIMARY_KEY) &&
        (f->flags & KFM_FLAG_AUTO_INCREMENT)) {
      int64_t *pk = (int64_t *)((char *)obj + f->offset);
      if (*pk == 0)
        continue;
    }

    if (!first)
      dbuf_append(&sql, ", ");
    dbuf_append(&sql, "%s", f->name);
    first = 0;
  }

  dbuf_append(&sql, ") VALUES (");

  /* Values */
  first = 1;
  for (int i = 0; i < model->num_fields; i++) {
    const kfm_field_t *f = &model->fields[i];

    if ((f->flags & KFM_FLAG_PRIMARY_KEY) &&
        (f->flags & KFM_FLAG_AUTO_INCREMENT)) {
      int64_t *pk = (int64_t *)((char *)obj + f->offset);
      if (*pk == 0)
        continue;
    }

    if (!first)
      dbuf_append(&sql, ", ");

    char *val = field_to_sql(model, f, obj);
    dbuf_append(&sql, "%s", val);
    free(val);
    first = 0;
  }

  dbuf_append(&sql, ")");

  // printf("DEBUG SQL: %s\n", sql.data);

  kdb_error_t err = kdb_execute(ctx->db, sql.data);
  dbuf_free(&sql);

  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

kfm_error_t kfm_update(kfm_ctx_t *ctx, const kfm_model_t *model, void *obj) {
  if (!ctx || !model || !obj)
    return KFM_ERR_INVALID;

  if (model->pk_field_idx < 0)
    return KFM_ERR_INVALID;

  const kfm_field_t *pk_field = &model->fields[model->pk_field_idx];
  int64_t pk_val = *(int64_t *)((char *)obj + pk_field->offset);

  /* Use dynamic buffer */
  dyn_buf_t sql;
  if (dbuf_init(&sql, 1024) < 0)
    return KFM_ERR_NOMEM;

  int res = dbuf_append(&sql, "UPDATE %s SET ", model->table_name);
  if (res < 0) {
    dbuf_free(&sql);
    return KFM_ERR_NOMEM;
  }

  int first = 1;
  for (int i = 0; i < model->num_fields; i++) {
    if (i == model->pk_field_idx)
      continue; /* Skip PK */

    const kfm_field_t *f = &model->fields[i];
    if (!first)
      dbuf_append(&sql, ", ");

    char *val = field_to_sql(model, f, obj);
    dbuf_append(&sql, "%s = %s", f->name, val);
    free(val);
    first = 0;
  }

  dbuf_append(&sql, " WHERE %s = %lld", pk_field->name, (long long)pk_val);

  kdb_error_t err = kdb_execute(ctx->db, sql.data);
  dbuf_free(&sql);

  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

kfm_error_t kfm_save(kfm_ctx_t *ctx, const kfm_model_t *model, void *obj) {
  if (!ctx || !model || !obj)
    return KFM_ERR_INVALID;

  if (model->pk_field_idx < 0)
    return kfm_insert(ctx, model, obj);

  const kfm_field_t *pk_field = &model->fields[model->pk_field_idx];
  int64_t pk_val = *(int64_t *)((char *)obj + pk_field->offset);

  if (pk_val == 0) {
    return kfm_insert(ctx, model, obj);
  } else {
    return kfm_update(ctx, model, obj);
  }
}

kfm_error_t kfm_delete(kfm_ctx_t *ctx, const kfm_model_t *model, void *obj) {
  if (!ctx || !model || !obj)
    return KFM_ERR_INVALID;

  if (model->pk_field_idx < 0)
    return KFM_ERR_INVALID;

  const kfm_field_t *pk_field = &model->fields[model->pk_field_idx];
  int64_t pk_val = *(int64_t *)((char *)obj + pk_field->offset);

  char sql[256];
  snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE %s = %lld",
           model->table_name, pk_field->name, (long long)pk_val);

  kdb_error_t err = kdb_execute(ctx->db, sql);
  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

/* ========== Query operations ========== */

/* Convert result set row to struct */
static void *row_to_struct(const kfm_model_t *model, kdb_result_t *result,
                           int row_idx) {
  void *obj = calloc(1, model->struct_size);
  if (!obj)
    return NULL;

  kdb_row_t *row = &result->rows[row_idx];

  for (int i = 0; i < result->num_columns && i < row->num_values; i++) {
    /* Find matching field in model */
    for (int j = 0; j < model->num_fields; j++) {
      if (strcmp(model->fields[j].name, result->columns[i]) == 0) {
        set_field_from_value(&model->fields[j], obj, &row->values[i]);
        break;
      }
    }
  }

  return obj;
}

void *kfm_find_by_id(kfm_ctx_t *ctx, const kfm_model_t *model, int64_t id) {
  if (!ctx || !model || model->pk_field_idx < 0)
    return NULL;

  const kfm_field_t *pk_field = &model->fields[model->pk_field_idx];

  char sql[512];
  snprintf(sql, sizeof(sql), "SELECT * FROM %s WHERE %s = %lld",
           model->table_name, pk_field->name, (long long)id);

  kdb_result_t *result;
  kdb_error_t err = kdb_query(ctx->db, sql, &result);
  if (err != KDB_OK || result->num_rows == 0) {
    if (result)
      kdb_result_free(result);
    return NULL;
  }

  void *obj = row_to_struct(model, result, 0);
  kdb_result_free(result);
  return obj;
}

kfm_list_t *kfm_find_all(kfm_ctx_t *ctx, const kfm_model_t *model) {
  return kfm_find_where(ctx, model, NULL);
}

kfm_list_t *kfm_find_where(kfm_ctx_t *ctx, const kfm_model_t *model,
                           const char *where) {
  if (!ctx || !model)
    return NULL;

  dyn_buf_t sql;
  if (dbuf_init(&sql, 1024) < 0)
    return NULL;

  if (where && strlen(where) > 0) {
    dbuf_append(&sql, "SELECT * FROM %s WHERE %s", model->table_name, where);
  } else {
    dbuf_append(&sql, "SELECT * FROM %s", model->table_name);
  }

  kdb_result_t *result;
  kdb_error_t err = kdb_query(ctx->db, sql.data, &result);
  dbuf_free(&sql);
  if (err != KDB_OK)
    return NULL;

  kfm_list_t *list = calloc(1, sizeof(kfm_list_t));
  if (!list) {
    kdb_result_free(result);
    return NULL;
  }

  list->model = model;
  list->count = result->num_rows;
  list->capacity = result->num_rows;
  list->items = calloc(list->capacity, sizeof(void *));

  if (!list->items) {
    free(list);
    kdb_result_free(result);
    return NULL;
  }

  for (int i = 0; i < result->num_rows; i++) {
    list->items[i] = row_to_struct(model, result, i);
  }

  kdb_result_free(result);
  return list;
}

void *kfm_find_one(kfm_ctx_t *ctx, const kfm_model_t *model,
                   const char *where) {
  kfm_list_t *list = kfm_find_where(ctx, model, where);
  if (!list || list->count == 0) {
    kfm_list_free(list);
    return NULL;
  }

  void *obj = list->items[0];
  list->items[0] = NULL; /* Prevent double-free */
  kfm_list_free(list);
  return obj;
}

int kfm_count(kfm_ctx_t *ctx, const kfm_model_t *model, const char *where) {
  if (!ctx || !model)
    return -1;

  /* Use find_where and count results since COUNT(*) may not be supported */
  kfm_list_t *list = kfm_find_where(ctx, model, where);
  if (!list)
    return -1;

  int count = list->count;
  kfm_list_free(list);
  return count;
}

kfm_error_t kfm_raw_execute(kfm_ctx_t *ctx, const char *sql) {
  if (!ctx || !sql)
    return KFM_ERR_INVALID;

  kdb_error_t err = kdb_execute(ctx->db, sql);
  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

kdb_result_t *kfm_raw_query(kfm_ctx_t *ctx, const char *sql) {
  if (!ctx || !sql)
    return NULL;

  kdb_result_t *result;
  kdb_error_t err = kdb_query(ctx->db, sql, &result);
  return (err == KDB_OK) ? result : NULL;
}

/* ========== List operations ========== */

void kfm_list_free(kfm_list_t *list) {
  if (!list)
    return;

  for (int i = 0; i < list->count; i++) {
    if (list->items[i]) {
      kfm_free(list->model, list->items[i]);
    }
  }
  free(list->items);
  free(list);
}

void *kfm_list_get(kfm_list_t *list, int index) {
  if (!list || index < 0 || index >= list->count)
    return NULL;
  return list->items[index];
}

/* ========== JSON conversion ========== */

json_value_t *kfm_to_json(const kfm_model_t *model, void *obj) {
  if (!model || !obj)
    return NULL;

  json_value_t *json = json_object_new();
  if (!json)
    return NULL;

  for (int i = 0; i < model->num_fields; i++) {
    const kfm_field_t *f = &model->fields[i];
    char *ptr = (char *)obj + f->offset;

    json_value_t *val = NULL;
    switch (f->type) {
    case KDB_TYPE_INTEGER:
    case KDB_TYPE_TIMESTAMP:
      val = json_int(*(int64_t *)ptr);
      break;

    case KDB_TYPE_REAL:
      val = json_number(*(double *)ptr);
      break;

    case KDB_TYPE_TEXT: {
      char **str_ptr = (char **)ptr;
      if (*str_ptr) {
        val = json_string(*str_ptr);
      } else {
        val = json_null();
      }
      break;
    }

    default:
      val = json_null();
      break;
    }

    if (val) {
      json_object_set(json, f->name, val);
    }
  }

  return json;
}

json_value_t *kfm_list_to_json(kfm_list_t *list) {
  if (!list)
    return json_array_new();

  json_value_t *arr = json_array_new();
  if (!arr)
    return NULL;

  for (int i = 0; i < list->count; i++) {
    json_value_t *item = kfm_to_json(list->model, list->items[i]);
    if (item) {
      json_array_push(arr, item);
    }
  }

  return arr;
}

kfm_error_t kfm_from_json(const kfm_model_t *model, void *obj,
                          json_value_t *json) {
  if (!model || !obj || !json || !json_is_object(json))
    return KFM_ERR_INVALID;

  for (int i = 0; i < model->num_fields; i++) {
    const kfm_field_t *f = &model->fields[i];
    char *ptr = (char *)obj + f->offset;

    json_value_t *val = json_object_get(json, f->name);
    if (!val)
      continue;

    switch (f->type) {
    case KDB_TYPE_INTEGER:
    case KDB_TYPE_TIMESTAMP:
      if (json_is_number(val)) {
        *(int64_t *)ptr = json_get_int(val);
      }
      break;

    case KDB_TYPE_REAL:
      if (json_is_number(val)) {
        *(double *)ptr = json_get_number(val);
      }
      break;

    case KDB_TYPE_TEXT: {
      char **str_ptr = (char **)ptr;
      /* Free existing string */
      free(*str_ptr);
      if (json_is_string(val)) {
        *str_ptr = strdup(json_get_string(val));
      } else {
        *str_ptr = NULL;
      }
      break;
    }

    default:
      break;
    }
  }

  return KFM_OK;
}

/* ========== Object lifecycle ========== */

void *kfm_new(const kfm_model_t *model) {
  if (!model)
    return NULL;
  return calloc(1, model->struct_size);
}

void kfm_free(const kfm_model_t *model, void *obj) {
  if (!model || !obj)
    return;

  /* Free string fields */
  for (int i = 0; i < model->num_fields; i++) {
    if (model->fields[i].type == KDB_TYPE_TEXT) {
      char **str_ptr = (char **)((char *)obj + model->fields[i].offset);
      free(*str_ptr);
    }
  }

  free(obj);
}

/* ========== Transaction helpers ========== */

kfm_error_t kfm_begin(kfm_ctx_t *ctx) {
  if (!ctx)
    return KFM_ERR_INVALID;
  kdb_error_t err = kdb_txn_begin(ctx->db);
  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

kfm_error_t kfm_commit(kfm_ctx_t *ctx) {
  if (!ctx)
    return KFM_ERR_INVALID;
  kdb_error_t err = kdb_txn_commit(ctx->db);
  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

kfm_error_t kfm_rollback(kfm_ctx_t *ctx) {
  if (!ctx)
    return KFM_ERR_INVALID;
  kdb_error_t err = kdb_txn_rollback(ctx->db);
  return (err == KDB_OK) ? KFM_OK : KFM_ERR_DB;
}

/* ========== Model builder ========== */

kfm_model_t *kfm_model_create(const char *table_name, size_t struct_size) {
  kfm_model_t *model = calloc(1, sizeof(kfm_model_t));
  if (!model)
    return NULL;

  model->table_name = table_name;
  model->struct_size = struct_size;
  model->pk_field_idx = -1;
  return model;
}

bool kfm_model_add_field(kfm_model_t *model, const char *name, kdb_type_t type,
                         size_t offset, size_t size, int flags) {
  if (!model || model->num_fields >= KDB_MAX_COLUMNS)
    return false;

  kfm_field_t *f = &model->fields[model->num_fields];
  f->name = name;
  f->type = type;
  f->offset = offset;
  f->size = size;
  f->flags = flags;

  if (flags & KFM_FLAG_PRIMARY_KEY) {
    model->pk_field_idx = model->num_fields;
  }

  model->num_fields++;
  return true;
}

void kfm_model_free(kfm_model_t *model) { free(model); }

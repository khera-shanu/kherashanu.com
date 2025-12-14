/**
 * Kherashanu ORM - SQLAlchemy-inspired database access layer
 */
#ifndef KFW_ORM_H
#define KFW_ORM_H

#include "../db/db.h"
#include "../json/json.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Error codes */
typedef enum {
  KFM_OK = 0,
  KFM_ERR_NOMEM,
  KFM_ERR_DB,
  KFM_ERR_NOT_FOUND,
  KFM_ERR_INVALID,
  KFM_ERR_CONSTRAINT
} kfm_error_t;

/* Field flags */
#define KFM_FLAG_PRIMARY_KEY (1 << 0)
#define KFM_FLAG_NOT_NULL (1 << 1)
#define KFM_FLAG_UNIQUE (1 << 2)
#define KFM_FLAG_AUTO_INCREMENT (1 << 3)

/* Field definition */
typedef struct {
  const char *name;
  kdb_type_t type;
  int flags;
  size_t offset; /* Offset in struct */
  size_t size;   /* Size of field */
} kfm_field_t;

/* Model metadata */
typedef struct {
  const char *table_name;
  size_t struct_size;
  int num_fields;
  kfm_field_t fields[KDB_MAX_COLUMNS];
  int pk_field_idx; /* Index of primary key field, or -1 */
} kfm_model_t;

/* ORM context */
typedef struct {
  kdb_t *db;
} kfm_ctx_t;

/* Result list */
typedef struct {
  void **items;
  int count;
  int capacity;
  const kfm_model_t *model; /* For cleanup */
} kfm_list_t;

/* ========== Context management ========== */

/**
 * Initialize ORM context with database
 */
kfm_ctx_t *kfm_init(kdb_t *db);

/**
 * Destroy ORM context
 */
void kfm_destroy(kfm_ctx_t *ctx);

/**
 * Get the underlying database handle
 */
kdb_t *kfm_get_db(kfm_ctx_t *ctx);

/* ========== Table operations ========== */

/**
 * Create table for model if it doesn't exist
 */
kfm_error_t kfm_create_table(kfm_ctx_t *ctx, const kfm_model_t *model);

/**
 * Drop table for model
 */
kfm_error_t kfm_drop_table(kfm_ctx_t *ctx, const kfm_model_t *model);

/* ========== CRUD operations ========== */

/**
 * Insert a new record
 * @param ctx   ORM context
 * @param model Model metadata
 * @param obj   Struct instance to insert
 * @return KFM_OK on success
 */
kfm_error_t kfm_insert(kfm_ctx_t *ctx, const kfm_model_t *model, void *obj);

/**
 * Update an existing record (by primary key)
 */
kfm_error_t kfm_update(kfm_ctx_t *ctx, const kfm_model_t *model, void *obj);

/**
 * Save record (insert if new, update if exists)
 */
kfm_error_t kfm_save(kfm_ctx_t *ctx, const kfm_model_t *model, void *obj);

/**
 * Delete a record (by primary key)
 */
kfm_error_t kfm_delete(kfm_ctx_t *ctx, const kfm_model_t *model, void *obj);

/**
 * Find by primary key
 * @return Allocated struct or NULL if not found
 */
void *kfm_find_by_id(kfm_ctx_t *ctx, const kfm_model_t *model, int64_t id);

/**
 * Find all records
 */
kfm_list_t *kfm_find_all(kfm_ctx_t *ctx, const kfm_model_t *model);

/**
 * Find with WHERE clause
 * @param where WHERE clause (without "WHERE" keyword)
 */
kfm_list_t *kfm_find_where(kfm_ctx_t *ctx, const kfm_model_t *model,
                           const char *where);

/**
 * Find first record matching WHERE clause
 */
void *kfm_find_one(kfm_ctx_t *ctx, const kfm_model_t *model, const char *where);

/**
 * Count records
 */
int kfm_count(kfm_ctx_t *ctx, const kfm_model_t *model, const char *where);

/**
 * Execute raw SQL (for complex queries)
 */
kfm_error_t kfm_raw_execute(kfm_ctx_t *ctx, const char *sql);

/**
 * Execute raw SQL query and return result
 */
kdb_result_t *kfm_raw_query(kfm_ctx_t *ctx, const char *sql);

/* ========== List operations ========== */

/**
 * Free list and all contained items
 */
void kfm_list_free(kfm_list_t *list);

/**
 * Get item from list
 */
void *kfm_list_get(kfm_list_t *list, int index);

/* ========== JSON conversion ========== */

/**
 * Convert model instance to JSON object
 */
json_value_t *kfm_to_json(const kfm_model_t *model, void *obj);

/**
 * Convert list to JSON array
 */
json_value_t *kfm_list_to_json(kfm_list_t *list);

/**
 * Populate model instance from JSON object
 */
kfm_error_t kfm_from_json(const kfm_model_t *model, void *obj,
                          json_value_t *json);

/* ========== Object lifecycle ========== */

/**
 * Allocate and zero-initialize a model instance
 */
void *kfm_new(const kfm_model_t *model);

/**
 * Free a model instance (and any string fields)
 */
void kfm_free(const kfm_model_t *model, void *obj);

/* ========== Helper macros ========== */

/* Get pointer to field in struct */
#define KFM_FIELD_PTR(obj, offset, type) ((type *)((char *)(obj) + (offset)))

/* Type mapping macros */
#define KFM_TYPE_INT KDB_TYPE_INTEGER
#define KFM_TYPE_INTEGER KDB_TYPE_INTEGER
#define KFM_TYPE_REAL KDB_TYPE_REAL
#define KFM_TYPE_FLOAT KDB_TYPE_REAL
#define KFM_TYPE_DOUBLE KDB_TYPE_REAL
#define KFM_TYPE_TEXT KDB_TYPE_TEXT
#define KFM_TYPE_STRING KDB_TYPE_TEXT
#define KFM_TYPE_BLOB KDB_TYPE_BLOB
#define KFM_TYPE_TIMESTAMP KDB_TYPE_TIMESTAMP

/* ========== Transaction helpers ========== */

/**
 * Begin transaction
 */
kfm_error_t kfm_begin(kfm_ctx_t *ctx);

/**
 * Commit transaction
 */
kfm_error_t kfm_commit(kfm_ctx_t *ctx);

/**
 * Rollback transaction
 */
kfm_error_t kfm_rollback(kfm_ctx_t *ctx);

/* ========== Model builder (alternative to macros) ========== */

/**
 * Create a model definition dynamically
 */
kfm_model_t *kfm_model_create(const char *table_name, size_t struct_size);

/**
 * Add field to model
 */
bool kfm_model_add_field(kfm_model_t *model, const char *name, kdb_type_t type,
                         size_t offset, size_t size, int flags);

/**
 * Free dynamically created model
 */
void kfm_model_free(kfm_model_t *model);

#endif /* KFW_ORM_H */

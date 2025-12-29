/**
 * Kherashanu Database - Lightweight SQL Database
 * Main interface header
 */
#ifndef KDB_H
#define KDB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Version */
#define KDB_VERSION_MAJOR 1
#define KDB_VERSION_MINOR 0

/* Limits */
#define KDB_MAX_COLUMNS 32
#define KDB_MAX_TABLE_NAME 64
#define KDB_MAX_COLUMN_NAME 64
#define KDB_MAX_TABLES 32
#define KDB_MAX_TEXT_LEN (8 * 1024 * 1024) /* 8MB max text length */
#define KDB_PAGE_SIZE (8 * 1024 * 1024)    /* 8MB page size for large content */

/* Error codes */
typedef enum {
  KDB_OK = 0,
  KDB_ERR_NOMEM,
  KDB_ERR_IO,
  KDB_ERR_CORRUPT,
  KDB_ERR_SYNTAX,
  KDB_ERR_NO_TABLE,
  KDB_ERR_NO_COLUMN,
  KDB_ERR_DUPLICATE,
  KDB_ERR_TYPE_MISMATCH,
  KDB_ERR_CONSTRAINT,
  KDB_ERR_TXN_ACTIVE,
  KDB_ERR_NO_TXN,
  KDB_ERR_LOCKED
} kdb_error_t;

/* Data types */
typedef enum {
  KDB_TYPE_NULL = 0,
  KDB_TYPE_INTEGER,
  KDB_TYPE_REAL,
  KDB_TYPE_TEXT,
  KDB_TYPE_BLOB,
  KDB_TYPE_TIMESTAMP
} kdb_type_t;

/* Value union */
typedef struct {
  kdb_type_t type;
  union {
    int64_t i; /* INTEGER, TIMESTAMP */
    double r;  /* REAL */
    struct {
      char *data;
      size_t len;
    } text; /* TEXT */
    struct {
      void *data;
      size_t len;
    } blob; /* BLOB */
  } v;
} kdb_value_t;

/* Column definition */
typedef struct {
  char name[KDB_MAX_COLUMN_NAME];
  kdb_type_t type;
  bool primary_key;
  bool unique;
  bool not_null;
} kdb_column_def_t;

/* Row - array of values */
typedef struct {
  kdb_value_t *values;
  int num_values;
} kdb_row_t;

/* Result set from queries */
typedef struct {
  char **columns; /* Column names */
  int num_columns;
  kdb_row_t *rows;
  int num_rows;
  int rows_affected;
  int64_t last_insert_id;
} kdb_result_t;

/* Forward declarations */
typedef struct kdb_s kdb_t;
typedef struct kdb_txn_s kdb_txn_t;

/**
 * Open or create a database
 * @param path Path to database file
 * @param db   Output database handle
 * @return KDB_OK on success
 */
kdb_error_t kdb_open(const char *path, kdb_t **db);

/**
 * Close database and free resources
 */
void kdb_close(kdb_t *db);

/**
 * Execute SQL statement (no result set)
 * @param db   Database handle
 * @param sql  SQL statement
 * @return KDB_OK on success
 */
kdb_error_t kdb_execute(kdb_t *db, const char *sql);

/**
 * Execute SQL query (returns result set)
 * @param db     Database handle
 * @param sql    SQL query
 * @param result Output result set (caller must free with kdb_result_free)
 * @return KDB_OK on success
 */
kdb_error_t kdb_query(kdb_t *db, const char *sql, kdb_result_t **result);

/**
 * Free result set
 */
void kdb_result_free(kdb_result_t *result);

/**
 * Get last error message
 */
const char *kdb_error_msg(kdb_t *db);

/**
 * Get last error code
 */
kdb_error_t kdb_error_code(kdb_t *db);

/* Transaction functions */
kdb_error_t kdb_txn_begin(kdb_t *db);
kdb_error_t kdb_txn_commit(kdb_t *db);
kdb_error_t kdb_txn_rollback(kdb_t *db);

/* Utility functions */
void kdb_value_free(kdb_value_t *val);
kdb_value_t kdb_value_int(int64_t i);
kdb_value_t kdb_value_real(double r);
kdb_value_t kdb_value_text(const char *text);
kdb_value_t kdb_value_null(void);

/* Storage access (for migrations) - returns opaque pointer */
void *kdb_get_storage(kdb_t *db);

#endif /* KDB_H */

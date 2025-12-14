/**
 * Storage Layer - Page-based Persistent Storage
 */
#ifndef KDB_STORAGE_H
#define KDB_STORAGE_H

#include "btree.h"
#include "db.h"
#include <pthread.h>
#include <stdio.h>

/* Page types */
typedef enum {
  PAGE_FREE = 0,
  PAGE_META,
  PAGE_TABLE_DEF,
  PAGE_DATA,
  PAGE_OVERFLOW
} page_type_t;

/* Page header (64 bytes) */
typedef struct {
  uint32_t page_id;
  page_type_t type;
  uint32_t next_page; /* For overflow/free list */
  uint32_t num_records;
  uint32_t free_space;
  uint8_t reserved[44];
} page_header_t;

/* Page structure */
typedef struct {
  page_header_t header;
  uint8_t data[KDB_PAGE_SIZE - sizeof(page_header_t)];
} page_t;

/* Table metadata */
typedef struct {
  char name[KDB_MAX_TABLE_NAME];
  kdb_column_def_t columns[KDB_MAX_COLUMNS];
  int num_columns;
  int pk_column; /* Index of primary key column, -1 if none */
  uint32_t first_data_page;
  uint32_t last_data_page;
  int64_t next_rowid;
  int64_t row_count;
  btree_t *pk_index; /* Primary key index */
} table_t;

/* Storage handle */
typedef struct {
  char path[512];
  FILE *file;
  table_t tables[KDB_MAX_TABLES];
  int num_tables;
  uint32_t num_pages;
  uint32_t free_page_head;
  pthread_mutex_t lock;
  bool dirty;
} storage_t;

/**
 * Open or create storage file
 */
kdb_error_t storage_open(const char *path, storage_t **store);

/**
 * Close storage and flush to disk
 */
void storage_close(storage_t *store);

/**
 * Sync changes to disk
 */
kdb_error_t storage_sync(storage_t *store);

/* Table operations */
kdb_error_t storage_create_table(storage_t *store, const char *name,
                                 const kdb_column_def_t *cols, int num_cols);
kdb_error_t storage_drop_table(storage_t *store, const char *name);
table_t *storage_get_table(storage_t *store, const char *name);

/* Row operations */
kdb_error_t storage_insert_row(storage_t *store, table_t *table,
                               const kdb_value_t *values, int num_values,
                               int64_t *rowid);
kdb_error_t storage_update_row(storage_t *store, table_t *table, int64_t rowid,
                               const kdb_value_t *values);
kdb_error_t storage_delete_row(storage_t *store, table_t *table, int64_t rowid);
kdb_error_t storage_get_row(storage_t *store, table_t *table, int64_t rowid,
                            kdb_row_t *row);

/* Iteration */
typedef struct storage_cursor_s storage_cursor_t;
storage_cursor_t *storage_cursor_create(storage_t *store, table_t *table);
bool storage_cursor_next(storage_cursor_t *cursor, int64_t *rowid,
                         kdb_row_t *row);
void storage_cursor_destroy(storage_cursor_t *cursor);

/*
 * Rebuild all indices from data pages
 */
kdb_error_t storage_rebuild_indices(storage_t *store);

#endif /* KDB_STORAGE_H */

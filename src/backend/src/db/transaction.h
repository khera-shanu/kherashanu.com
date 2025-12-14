/**
 * Transaction Manager with Write-Ahead Logging
 */
#ifndef KDB_TRANSACTION_H
#define KDB_TRANSACTION_H

#include "db.h"
#include "storage.h"

/* WAL record types */
typedef enum {
  WAL_BEGIN,
  WAL_COMMIT,
  WAL_ROLLBACK,
  WAL_INSERT,
  WAL_UPDATE,
  WAL_DELETE
} wal_record_type_t;

/* WAL record header */
typedef struct {
  uint32_t record_size;
  uint32_t txn_id;
  wal_record_type_t type;
  uint32_t checksum;
} wal_header_t;

/* Transaction state */
typedef enum {
  TXN_NONE,
  TXN_ACTIVE,
  TXN_COMMITTED,
  TXN_ROLLED_BACK
} txn_state_t;

/* Transaction handle */
typedef struct {
  uint32_t txn_id;
  txn_state_t state;
  /* Undo log for rollback */
  struct {
    char table[KDB_MAX_TABLE_NAME];
    int64_t rowid;
    kdb_row_t *old_data; /* NULL for INSERT */
    bool was_insert;
  } *undo_log;
  int undo_count;
  int undo_capacity;
} txn_t;

/* Transaction manager */
typedef struct {
  storage_t *store;
  FILE *wal_file;
  char wal_path[520];
  uint32_t next_txn_id;
  txn_t *current_txn;
  pthread_mutex_t lock;
} txn_manager_t;

/**
 * Initialize transaction manager
 */
kdb_error_t txn_manager_init(txn_manager_t *mgr, storage_t *store,
                             const char *db_path);

/**
 * Cleanup transaction manager
 */
void txn_manager_cleanup(txn_manager_t *mgr);

/**
 * Recover from WAL after crash
 */
kdb_error_t txn_recover(txn_manager_t *mgr);

/**
 * Begin new transaction
 */
kdb_error_t txn_begin(txn_manager_t *mgr);

/**
 * Commit current transaction
 */
kdb_error_t txn_commit(txn_manager_t *mgr);

/**
 * Rollback current transaction
 */
kdb_error_t txn_rollback(txn_manager_t *mgr);

/**
 * Log an operation (called during INSERT/UPDATE/DELETE)
 */
kdb_error_t txn_log_insert(txn_manager_t *mgr, const char *table,
                           int64_t rowid);
kdb_error_t txn_log_update(txn_manager_t *mgr, const char *table, int64_t rowid,
                           const kdb_row_t *old_data);
kdb_error_t txn_log_delete(txn_manager_t *mgr, const char *table, int64_t rowid,
                           const kdb_row_t *old_data);

/**
 * Checkpoint - flush WAL to main database
 */
kdb_error_t txn_checkpoint(txn_manager_t *mgr);

#endif /* KDB_TRANSACTION_H */

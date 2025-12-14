/**
 * Transaction Manager Implementation
 */
#include "transaction.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Initialize transaction manager
 */
kdb_error_t txn_manager_init(txn_manager_t *mgr, storage_t *store,
                             const char *db_path) {
  if (!mgr || !store || !db_path)
    return KDB_ERR_IO;

  memset(mgr, 0, sizeof(*mgr));
  mgr->store = store;
  mgr->next_txn_id = 1;
  pthread_mutex_init(&mgr->lock, NULL);

  /* Create WAL file path */
  snprintf(mgr->wal_path, sizeof(mgr->wal_path), "%s.wal", db_path);

  /* Open or create WAL file */
  mgr->wal_file = fopen(mgr->wal_path, "a+b");
  if (!mgr->wal_file) {
    return KDB_ERR_IO;
  }

  return KDB_OK;
}

/*
 * Cleanup transaction manager
 */
void txn_manager_cleanup(txn_manager_t *mgr) {
  if (!mgr)
    return;

  if (mgr->current_txn) {
    txn_rollback(mgr);
  }

  if (mgr->wal_file) {
    fclose(mgr->wal_file);
  }

  pthread_mutex_destroy(&mgr->lock);
}

/*
 * Begin transaction
 */
kdb_error_t txn_begin(txn_manager_t *mgr) {
  if (!mgr)
    return KDB_ERR_IO;

  pthread_mutex_lock(&mgr->lock);

  if (mgr->current_txn) {
    pthread_mutex_unlock(&mgr->lock);
    return KDB_ERR_TXN_ACTIVE;
  }

  mgr->current_txn = calloc(1, sizeof(txn_t));
  if (!mgr->current_txn) {
    pthread_mutex_unlock(&mgr->lock);
    return KDB_ERR_NOMEM;
  }

  mgr->current_txn->txn_id = mgr->next_txn_id++;
  mgr->current_txn->state = TXN_ACTIVE;
  mgr->current_txn->undo_capacity = 16;
  mgr->current_txn->undo_log = calloc(mgr->current_txn->undo_capacity,
                                      sizeof(*mgr->current_txn->undo_log));

  /* Write BEGIN to WAL */
  wal_header_t hdr = {.record_size = sizeof(wal_header_t),
                      .txn_id = mgr->current_txn->txn_id,
                      .type = WAL_BEGIN,
                      .checksum = 0};
  fwrite(&hdr, sizeof(hdr), 1, mgr->wal_file);
  fflush(mgr->wal_file);

  pthread_mutex_unlock(&mgr->lock);
  return KDB_OK;
}

/*
 * Commit transaction
 */
kdb_error_t txn_commit(txn_manager_t *mgr) {
  if (!mgr)
    return KDB_ERR_IO;

  pthread_mutex_lock(&mgr->lock);

  if (!mgr->current_txn) {
    pthread_mutex_unlock(&mgr->lock);
    return KDB_ERR_NO_TXN;
  }

  /* Write COMMIT to WAL */
  wal_header_t hdr = {.record_size = sizeof(wal_header_t),
                      .txn_id = mgr->current_txn->txn_id,
                      .type = WAL_COMMIT,
                      .checksum = 0};
  fwrite(&hdr, sizeof(hdr), 1, mgr->wal_file);
  fflush(mgr->wal_file);

  /* Sync storage */
  storage_sync(mgr->store);

  mgr->current_txn->state = TXN_COMMITTED;

  /* Free undo log */
  for (int i = 0; i < mgr->current_txn->undo_count; i++) {
    if (mgr->current_txn->undo_log[i].old_data) {
      for (int j = 0; j < mgr->current_txn->undo_log[i].old_data->num_values;
           j++) {
        kdb_value_free(&mgr->current_txn->undo_log[i].old_data->values[j]);
      }
      free(mgr->current_txn->undo_log[i].old_data->values);
      free(mgr->current_txn->undo_log[i].old_data);
    }
  }
  free(mgr->current_txn->undo_log);
  free(mgr->current_txn);
  mgr->current_txn = NULL;

  pthread_mutex_unlock(&mgr->lock);
  return KDB_OK;
}

/*
 * Rollback transaction
 */
kdb_error_t txn_rollback(txn_manager_t *mgr) {
  if (!mgr)
    return KDB_ERR_IO;

  pthread_mutex_lock(&mgr->lock);

  if (!mgr->current_txn) {
    pthread_mutex_unlock(&mgr->lock);
    return KDB_ERR_NO_TXN;
  }

  /* Apply undo log in reverse */
  for (int i = mgr->current_txn->undo_count - 1; i >= 0; i--) {
    table_t *table =
        storage_get_table(mgr->store, mgr->current_txn->undo_log[i].table);
    if (!table)
      continue;

    if (mgr->current_txn->undo_log[i].was_insert) {
      /* Undo insert = delete */
      storage_delete_row(mgr->store, table,
                         mgr->current_txn->undo_log[i].rowid);
    } else if (mgr->current_txn->undo_log[i].old_data) {
      /* Undo update/delete = restore old data */
      /* Simplified: would need full re-insert logic */
    }
  }

  /* Write ROLLBACK to WAL */
  wal_header_t hdr = {.record_size = sizeof(wal_header_t),
                      .txn_id = mgr->current_txn->txn_id,
                      .type = WAL_ROLLBACK,
                      .checksum = 0};
  fwrite(&hdr, sizeof(hdr), 1, mgr->wal_file);
  fflush(mgr->wal_file);

  mgr->current_txn->state = TXN_ROLLED_BACK;

  /* Free undo log */
  for (int i = 0; i < mgr->current_txn->undo_count; i++) {
    if (mgr->current_txn->undo_log[i].old_data) {
      for (int j = 0; j < mgr->current_txn->undo_log[i].old_data->num_values;
           j++) {
        kdb_value_free(&mgr->current_txn->undo_log[i].old_data->values[j]);
      }
      free(mgr->current_txn->undo_log[i].old_data->values);
      free(mgr->current_txn->undo_log[i].old_data);
    }
  }
  free(mgr->current_txn->undo_log);
  free(mgr->current_txn);
  mgr->current_txn = NULL;

  pthread_mutex_unlock(&mgr->lock);
  return KDB_OK;
}

/*
 * Log insert for undo
 */
kdb_error_t txn_log_insert(txn_manager_t *mgr, const char *table,
                           int64_t rowid) {
  if (!mgr || !mgr->current_txn)
    return KDB_OK; /* No active transaction */

  pthread_mutex_lock(&mgr->lock);

  txn_t *txn = mgr->current_txn;

  if (txn->undo_count >= txn->undo_capacity) {
    txn->undo_capacity *= 2;
    txn->undo_log =
        realloc(txn->undo_log, txn->undo_capacity * sizeof(*txn->undo_log));
  }

  strncpy(txn->undo_log[txn->undo_count].table, table, KDB_MAX_TABLE_NAME - 1);
  txn->undo_log[txn->undo_count].rowid = rowid;
  txn->undo_log[txn->undo_count].old_data = NULL;
  txn->undo_log[txn->undo_count].was_insert = true;
  txn->undo_count++;

  pthread_mutex_unlock(&mgr->lock);
  return KDB_OK;
}

/*
 * Log update for undo
 */
kdb_error_t txn_log_update(txn_manager_t *mgr, const char *table, int64_t rowid,
                           const kdb_row_t *old_data) {
  if (!mgr || !mgr->current_txn)
    return KDB_OK;

  pthread_mutex_lock(&mgr->lock);

  txn_t *txn = mgr->current_txn;

  if (txn->undo_count >= txn->undo_capacity) {
    txn->undo_capacity *= 2;
    txn->undo_log =
        realloc(txn->undo_log, txn->undo_capacity * sizeof(*txn->undo_log));
  }

  strncpy(txn->undo_log[txn->undo_count].table, table, KDB_MAX_TABLE_NAME - 1);
  txn->undo_log[txn->undo_count].rowid = rowid;
  txn->undo_log[txn->undo_count].was_insert = false;

  /* Copy old data */
  if (old_data) {
    txn->undo_log[txn->undo_count].old_data = malloc(sizeof(kdb_row_t));
    txn->undo_log[txn->undo_count].old_data->num_values = old_data->num_values;
    txn->undo_log[txn->undo_count].old_data->values =
        calloc(old_data->num_values, sizeof(kdb_value_t));
    /* Deep copy values - simplified */
  }

  txn->undo_count++;

  pthread_mutex_unlock(&mgr->lock);
  return KDB_OK;
}

/*
 * Log delete for undo
 */
kdb_error_t txn_log_delete(txn_manager_t *mgr, const char *table, int64_t rowid,
                           const kdb_row_t *old_data) {
  return txn_log_update(mgr, table, rowid, old_data);
}

/*
 * Recover from WAL (called on open)
 */
kdb_error_t txn_recover(txn_manager_t *mgr) {
  if (!mgr || !mgr->wal_file)
    return KDB_ERR_IO;

  fseek(mgr->wal_file, 0, SEEK_SET);

  wal_header_t hdr;
  while (fread(&hdr, sizeof(hdr), 1, mgr->wal_file) == 1) {
    /* Skip over record data */
    if (hdr.record_size > sizeof(hdr)) {
      fseek(mgr->wal_file, hdr.record_size - sizeof(hdr), SEEK_CUR);
    }

    /* Track transaction state for recovery */
    if (hdr.type == WAL_BEGIN) {
      if (hdr.txn_id >= mgr->next_txn_id) {
        mgr->next_txn_id = hdr.txn_id + 1;
      }
    }
  }

  return KDB_OK;
}

/*
 * Checkpoint - truncate WAL
 */
kdb_error_t txn_checkpoint(txn_manager_t *mgr) {
  if (!mgr)
    return KDB_ERR_IO;

  pthread_mutex_lock(&mgr->lock);

  /* Sync all data */
  storage_sync(mgr->store);

  /* Truncate WAL */
  fclose(mgr->wal_file);
  mgr->wal_file = fopen(mgr->wal_path, "wb");
  if (mgr->wal_file) {
    fclose(mgr->wal_file);
    mgr->wal_file = fopen(mgr->wal_path, "a+b");
  }

  pthread_mutex_unlock(&mgr->lock);
  return KDB_OK;
}

/**
 * Storage Layer Implementation - Page-based Persistent Storage
 */
#include "storage.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* File header (first page) */
typedef struct {
  char magic[8]; /* "KDBFILE" */
  uint32_t version;
  uint32_t page_size;
  uint32_t num_pages;
  uint32_t num_tables;
  uint32_t free_page_head;
  uint8_t reserved[KDB_PAGE_SIZE - 28];
} file_header_t;

static const char FILE_MAGIC[] = "KDBFILE";
static const uint32_t FILE_VERSION = 1;

/* Row format in page:
 * [2 bytes: row size][8 bytes: rowid][values...]
 * Value format:
 * [1 byte: type][data based on type]
 */

/*
 * Open or create storage file
 */
kdb_error_t storage_open(const char *path, storage_t **store) {
  storage_t *s = calloc(1, sizeof(storage_t));
  if (!s)
    return KDB_ERR_NOMEM;

  strncpy(s->path, path, sizeof(s->path) - 1);
  pthread_mutex_init(&s->lock, NULL);

  /* Try to open existing file */
  s->file = fopen(path, "r+b");
  if (s->file) {
    /* Read header */
    file_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, s->file) != 1) {
      fclose(s->file);
      free(s);
      return KDB_ERR_IO;
    }

    if (memcmp(hdr.magic, FILE_MAGIC, 7) != 0) {
      fclose(s->file);
      free(s);
      return KDB_ERR_CORRUPT;
    }

    s->num_pages = hdr.num_pages;
    s->num_tables = hdr.num_tables;
    s->free_page_head = hdr.free_page_head;

    /* Load table definitions */
    for (int i = 0; i < (int)s->num_tables; i++) {
      page_t page;
      fseek(s->file, (1 + i) * KDB_PAGE_SIZE, SEEK_SET);
      if (fread(&page, sizeof(page), 1, s->file) != 1) {
        fclose(s->file);
        free(s);
        return KDB_ERR_IO;
      }

      if (page.header.type != PAGE_TABLE_DEF)
        continue;

      table_t *t = &s->tables[i];
      memcpy(t, page.data, sizeof(table_t) - sizeof(btree_t *));

      /* Rebuild primary key index */
      if (t->pk_column >= 0) {
        t->pk_index = btree_create(true);
      }
    }

    /* Rebuild indices from data */
    storage_rebuild_indices(s);
  } else {
    /* Create new file */
    s->file = fopen(path, "w+b");
    if (!s->file) {
      free(s);
      return KDB_ERR_IO;
    }

    /* Write header */
    file_header_t hdr = {0};
    memcpy(hdr.magic, FILE_MAGIC, 7);
    hdr.version = FILE_VERSION;
    hdr.page_size = KDB_PAGE_SIZE;
    hdr.num_pages = 1;
    hdr.num_tables = 0;
    hdr.free_page_head = 0;

    if (fwrite(&hdr, KDB_PAGE_SIZE, 1, s->file) != 1) {
      fclose(s->file);
      free(s);
      return KDB_ERR_IO;
    }

    s->num_pages =
        1 + KDB_MAX_TABLES; /* Reserve pages for header + table defs */
    s->num_tables = 0;
    s->free_page_head = 0;
  }

  *store = s;
  return KDB_OK;
}

/*
 * Sync header to disk
 */
static kdb_error_t sync_header(storage_t *store) {
  file_header_t hdr = {0};
  memcpy(hdr.magic, FILE_MAGIC, 7);
  hdr.version = FILE_VERSION;
  hdr.page_size = KDB_PAGE_SIZE;
  hdr.num_pages = store->num_pages;
  hdr.num_tables = store->num_tables;
  hdr.free_page_head = store->free_page_head;

  fseek(store->file, 0, SEEK_SET);
  if (fwrite(&hdr, KDB_PAGE_SIZE, 1, store->file) != 1) {
    return KDB_ERR_IO;
  }
  return KDB_OK;
}

/*
 * Close storage
 */
void storage_close(storage_t *store) {
  if (!store)
    return;

  pthread_mutex_lock(&store->lock);

  /* Save all table metadata */
  for (int i = 0; i < store->num_tables; i++) {
    table_t *t = &store->tables[i];

    page_t page = {0};
    page.header.page_id = 1 + i;
    page.header.type = PAGE_TABLE_DEF;
    memcpy(page.data, t, sizeof(table_t) - sizeof(btree_t *));

    fseek(store->file, page.header.page_id * KDB_PAGE_SIZE, SEEK_SET);
    fwrite(&page, sizeof(page), 1, store->file);

    if (t->pk_index) {
      btree_destroy(t->pk_index);
    }
  }

  sync_header(store);
  fflush(store->file);
  fclose(store->file);

  pthread_mutex_unlock(&store->lock);
  pthread_mutex_destroy(&store->lock);
  free(store);
}

/*
 * Sync to disk
 */
kdb_error_t storage_sync(storage_t *store) {
  if (!store)
    return KDB_ERR_IO;

  pthread_mutex_lock(&store->lock);

  /* Save table metadata */
  for (int i = 0; i < store->num_tables; i++) {
    table_t *t = &store->tables[i];

    page_t page = {0};
    page.header.page_id = 1 + i;
    page.header.type = PAGE_TABLE_DEF;
    memcpy(page.data, t, sizeof(table_t) - sizeof(btree_t *));

    fseek(store->file, page.header.page_id * KDB_PAGE_SIZE, SEEK_SET);
    fwrite(&page, sizeof(page), 1, store->file);
  }

  kdb_error_t err = sync_header(store);
  fflush(store->file);

  pthread_mutex_unlock(&store->lock);
  return err;
}

/*
 * Allocate a new page
 */
static uint32_t alloc_page(storage_t *store) {
  uint32_t page_id;

  if (store->free_page_head) {
    /* Reuse from free list */
    page_id = store->free_page_head;

    page_t page;
    fseek(store->file, page_id * KDB_PAGE_SIZE, SEEK_SET);
    if (fread(&page, sizeof(page), 1, store->file) == 1) {
      store->free_page_head = page.header.next_page;
    }
  } else {
    /* Allocate new page at end */
    page_id = store->num_pages++;

    /* Extend file */
    page_t empty = {0};
    empty.header.page_id = page_id;
    fseek(store->file, page_id * KDB_PAGE_SIZE, SEEK_SET);
    fwrite(&empty, sizeof(empty), 1, store->file);
  }

  return page_id;
}

/*
 * Create a new table
 */
kdb_error_t storage_create_table(storage_t *store, const char *name,
                                 const kdb_column_def_t *cols, int num_cols) {
  if (!store || !name || !cols)
    return KDB_ERR_IO;
  if (store->num_tables >= KDB_MAX_TABLES)
    return KDB_ERR_CONSTRAINT;

  pthread_mutex_lock(&store->lock);

  /* Check for duplicate */
  for (int i = 0; i < store->num_tables; i++) {
    if (strcasecmp(store->tables[i].name, name) == 0) {
      pthread_mutex_unlock(&store->lock);
      return KDB_ERR_DUPLICATE;
    }
  }

  /* Create table entry */
  table_t *t = &store->tables[store->num_tables];
  memset(t, 0, sizeof(*t));
  strncpy(t->name, name, sizeof(t->name) - 1);

  t->pk_column = -1;
  for (int i = 0; i < num_cols && i < KDB_MAX_COLUMNS; i++) {
    memcpy(&t->columns[i], &cols[i], sizeof(kdb_column_def_t));
    if (cols[i].primary_key) {
      t->pk_column = i;
    }
  }
  t->num_columns = num_cols;

  /* Allocate first data page */
  t->first_data_page = alloc_page(store);
  t->last_data_page = t->first_data_page;
  t->next_rowid = 1;
  t->row_count = 0;

  /* Initialize data page */
  page_t page = {0};
  page.header.page_id = t->first_data_page;
  page.header.type = PAGE_DATA;
  page.header.free_space = sizeof(page.data);
  fseek(store->file, t->first_data_page * KDB_PAGE_SIZE, SEEK_SET);
  fwrite(&page, sizeof(page), 1, store->file);
  fflush(store->file);

  /* Create primary key index */
  if (t->pk_column >= 0) {
    t->pk_index = btree_create(true);
  }

  store->num_tables++;
  sync_header(store);
  fflush(store->file);

  pthread_mutex_unlock(&store->lock);
  return KDB_OK;
}

/*
 * Drop a table
 */
kdb_error_t storage_drop_table(storage_t *store, const char *name) {
  if (!store || !name)
    return KDB_ERR_IO;

  pthread_mutex_lock(&store->lock);

  int idx = -1;
  for (int i = 0; i < store->num_tables; i++) {
    if (strcasecmp(store->tables[i].name, name) == 0) {
      idx = i;
      break;
    }
  }

  if (idx < 0) {
    pthread_mutex_unlock(&store->lock);
    return KDB_ERR_NO_TABLE;
  }

  table_t *t = &store->tables[idx];

  /* Free data pages */
  uint32_t page_id = t->first_data_page;
  while (page_id) {
    page_t page;
    fseek(store->file, page_id * KDB_PAGE_SIZE, SEEK_SET);
    if (fread(&page, sizeof(page), 1, store->file) != 1)
      break;

    uint32_t next = page.header.next_page;

    /* Add to free list */
    page.header.type = PAGE_FREE;
    page.header.next_page = store->free_page_head;
    fseek(store->file, page_id * KDB_PAGE_SIZE, SEEK_SET);
    fwrite(&page, sizeof(page), 1, store->file);
    store->free_page_head = page_id;

    page_id = next;
  }

  /* Free index */
  if (t->pk_index) {
    btree_destroy(t->pk_index);
  }

  /* Remove from tables array */
  for (int i = idx; i < store->num_tables - 1; i++) {
    memcpy(&store->tables[i], &store->tables[i + 1], sizeof(table_t));
  }
  store->num_tables--;

  sync_header(store);

  pthread_mutex_unlock(&store->lock);
  return KDB_OK;
}

/*
 * Get table by name
 */
table_t *storage_get_table(storage_t *store, const char *name) {
  if (!store || !name)
    return NULL;

  for (int i = 0; i < store->num_tables; i++) {
    if (strcasecmp(store->tables[i].name, name) == 0) {
      return &store->tables[i];
    }
  }
  return NULL;
}

/*
 * Serialize value to buffer
 * Returns bytes written
 */
static size_t serialize_value(const kdb_value_t *val, uint8_t *buf,
                              size_t max) {
  if (max < 1)
    return 0;

  buf[0] = (uint8_t)val->type;
  size_t pos = 1;

  switch (val->type) {
  case KDB_TYPE_NULL:
    break;
  case KDB_TYPE_INTEGER:
  case KDB_TYPE_TIMESTAMP:
    if (pos + 8 > max)
      return 0;
    memcpy(buf + pos, &val->v.i, 8);
    pos += 8;
    break;
  case KDB_TYPE_REAL:
    if (pos + 8 > max)
      return 0;
    memcpy(buf + pos, &val->v.r, 8);
    pos += 8;
    break;
  case KDB_TYPE_TEXT:
    if (pos + 4 + val->v.text.len > max)
      return 0;
    {
      uint32_t len = (uint32_t)val->v.text.len;
      memcpy(buf + pos, &len, 4);
      pos += 4;
      memcpy(buf + pos, val->v.text.data, len);
      pos += len;
    }
    break;
  case KDB_TYPE_BLOB:
    if (pos + 4 + val->v.blob.len > max)
      return 0;
    {
      uint32_t len = (uint32_t)val->v.blob.len;
      memcpy(buf + pos, &len, 4);
      pos += 4;
      memcpy(buf + pos, val->v.blob.data, len);
      pos += len;
    }
    break;
  }

  return pos;
}

/*
 * Deserialize value from buffer
 * Returns bytes read
 */
static size_t deserialize_value(const uint8_t *buf, size_t max,
                                kdb_value_t *val) {
  if (max < 1)
    return 0;

  val->type = (kdb_type_t)buf[0];
  size_t pos = 1;

  switch (val->type) {
  case KDB_TYPE_NULL:
    break;
  case KDB_TYPE_INTEGER:
  case KDB_TYPE_TIMESTAMP:
    if (pos + 8 > max)
      return 0;
    memcpy(&val->v.i, buf + pos, 8);
    pos += 8;
    break;
  case KDB_TYPE_REAL:
    if (pos + 8 > max)
      return 0;
    memcpy(&val->v.r, buf + pos, 8);
    pos += 8;
    break;
  case KDB_TYPE_TEXT:
    if (pos + 4 > max)
      return 0;
    {
      uint32_t len;
      memcpy(&len, buf + pos, 4);
      pos += 4;
      if (pos + len > max)
        return 0;
      val->v.text.data = malloc(len + 1);
      if (!val->v.text.data)
        return 0;
      memcpy(val->v.text.data, buf + pos, len);
      val->v.text.data[len] = '\0';
      val->v.text.len = len;
      pos += len;
    }
    break;
  case KDB_TYPE_BLOB:
    if (pos + 4 > max)
      return 0;
    {
      uint32_t len;
      memcpy(&len, buf + pos, 4);
      pos += 4;
      if (pos + len > max)
        return 0;
      val->v.blob.data = malloc(len);
      if (!val->v.blob.data)
        return 0;
      memcpy(val->v.blob.data, buf + pos, len);
      val->v.blob.len = len;
      pos += len;
    }
    break;
  }

  return pos;
}

/*
 * Insert a row
 */
kdb_error_t storage_insert_row(storage_t *store, table_t *table,
                               const kdb_value_t *values, int num_values,
                               int64_t *rowid) {
  if (!store || !table || !values)
    return KDB_ERR_IO;

  pthread_mutex_lock(&store->lock);

  /* Serialize row */
  uint8_t row_buf[4096];
  size_t pos = 0;

  int64_t rid = table->next_rowid++;

  for (int i = 0; i < num_values && i < table->num_columns; i++) {
    kdb_value_t start_val = values[i];

    /* If this is the PK column and value is 0 or NULL, use the rowid */
    if (i == table->pk_column && table->columns[i].primary_key &&
        ((start_val.type == KDB_TYPE_INTEGER && start_val.v.i == 0) ||
         start_val.type == KDB_TYPE_NULL)) {
      start_val.type = KDB_TYPE_INTEGER;
      start_val.v.i = rid;
    }

    size_t written =
        serialize_value(&start_val, row_buf + pos, sizeof(row_buf) - pos);
    if (written == 0) {
      pthread_mutex_unlock(&store->lock);
      return KDB_ERR_IO;
    }
    pos += written;
  }

  /* Find page with space */
  page_t page;
  fseek(store->file, table->last_data_page * KDB_PAGE_SIZE, SEEK_SET);
  if (fread(&page, sizeof(page), 1, store->file) != 1) {
    pthread_mutex_unlock(&store->lock);
    return KDB_ERR_IO;
  }

  size_t row_size = 2 + 8 + pos; /* size + rowid + data */

  if (page.header.free_space < row_size) {
    /* Allocate new page */
    uint32_t new_page = alloc_page(store);
    page.header.next_page = new_page;
    fseek(store->file, table->last_data_page * KDB_PAGE_SIZE, SEEK_SET);
    fwrite(&page, sizeof(page), 1, store->file);

    table->last_data_page = new_page;

    memset(&page, 0, sizeof(page));
    page.header.page_id = new_page;
    page.header.type = PAGE_DATA;
    page.header.free_space = sizeof(page.data);
  }

  /* Write row to page */
  size_t offset = sizeof(page.data) - page.header.free_space;
  uint16_t size16 = (uint16_t)pos;
  memcpy(page.data + offset, &size16, 2);
  memcpy(page.data + offset + 2, &rid, 8);
  memcpy(page.data + offset + 10, row_buf, pos);

  page.header.free_space -= row_size;
  page.header.num_records++;

  fseek(store->file, table->last_data_page * KDB_PAGE_SIZE, SEEK_SET);
  fwrite(&page, sizeof(page), 1, store->file);
  fflush(store->file);

  /* Update index */
  if (table->pk_index && table->pk_column >= 0) {
    const kdb_value_t *pk_val = &values[table->pk_column];
    btree_key_t key;
    if (pk_val->type == KDB_TYPE_INTEGER) {
      key = btree_key_int(pk_val->v.i);
    } else if (pk_val->type == KDB_TYPE_TEXT) {
      key = btree_key_str(pk_val->v.text.data);
    } else {
      key = btree_key_int(rid);
    }
    btree_insert(table->pk_index, key, rid);
    btree_key_free(&key);
  }

  table->row_count++;
  if (rowid)
    *rowid = rid;

  /* Save/Sync Metadata (row count, next_rowid, page pointers) */
  page_t table_def_page = {0};
  table_def_page.header.page_id =
      1 + (table - store->tables); /* Pointer arithmetic to find index */
  table_def_page.header.type = PAGE_TABLE_DEF;
  memcpy(table_def_page.data, table, sizeof(table_t) - sizeof(btree_t *));

  fseek(store->file, table_def_page.header.page_id * KDB_PAGE_SIZE, SEEK_SET);
  fwrite(&table_def_page, sizeof(table_def_page), 1, store->file);
  fflush(store->file);

  pthread_mutex_unlock(&store->lock);
  return KDB_OK;
}

/*
 * Get a row by rowid
 */
kdb_error_t storage_get_row(storage_t *store, table_t *table, int64_t rowid,
                            kdb_row_t *row) {
  if (!store || !table || !row)
    return KDB_ERR_IO;

  pthread_mutex_lock(&store->lock);

  uint32_t page_id = table->first_data_page;

  while (page_id) {
    page_t page;
    fseek(store->file, page_id * KDB_PAGE_SIZE, SEEK_SET);
    if (fread(&page, sizeof(page), 1, store->file) != 1)
      break;

    /* Scan rows in page */
    size_t offset = 0;
    for (uint32_t i = 0; i < page.header.num_records; i++) {
      uint16_t size;
      int64_t rid;
      memcpy(&size, page.data + offset, 2);
      memcpy(&rid, page.data + offset + 2, 8);

      if (rid == rowid) {
        /* Found it! Deserialize values */
        row->num_values = table->num_columns;
        row->values = calloc(row->num_values, sizeof(kdb_value_t));

        size_t pos = offset + 10;
        for (int j = 0; j < row->num_values; j++) {
          size_t read = deserialize_value(
              page.data + pos, size - (pos - offset - 10), &row->values[j]);
          if (read == 0)
            break;
          pos += read;
        }

        pthread_mutex_unlock(&store->lock);
        return KDB_OK;
      }

      offset += 2 + 8 + size;
    }

    page_id = page.header.next_page;
  }

  pthread_mutex_unlock(&store->lock);
  return KDB_ERR_NO_TABLE; /* Row not found */
}

/*
 * Delete a row
 */
kdb_error_t storage_delete_row(storage_t *store, table_t *table,
                               int64_t rowid) {
  if (!store || !table)
    return KDB_ERR_IO;

  pthread_mutex_lock(&store->lock);

  uint32_t page_id = table->first_data_page;

  while (page_id) {
    page_t page;
    fseek(store->file, page_id * KDB_PAGE_SIZE, SEEK_SET);
    if (fread(&page, sizeof(page), 1, store->file) != 1)
      break;

    /* Scan rows in page */
    size_t offset = 0;
    for (uint32_t i = 0; i < page.header.num_records; i++) {
      uint16_t size;
      int64_t rid;
      memcpy(&size, page.data + offset, 2);
      memcpy(&rid, page.data + offset + 2, 8);

      if (rid == rowid) {
        /* Mark as deleted (set rowid to -1) */
        rid = -1;

        memcpy(page.data + offset + 2, &rid, 8);

        fseek(store->file, page_id * KDB_PAGE_SIZE, SEEK_SET);
        fwrite(&page, sizeof(page), 1, store->file);
        fflush(store->file);

        /* Remove from index */
        if (table->pk_index) {
          kdb_value_t *values = calloc(table->num_columns, sizeof(kdb_value_t));
          if (values) {
            size_t pos = offset + 10;
            for (int j = 0; j < table->num_columns; j++) {
              size_t read = deserialize_value(
                  page.data + pos, size - (pos - offset - 10), &values[j]);
              if (read == 0)
                break;
              pos += read;
            }

            if (table->pk_column >= 0 &&
                table->pk_column < table->num_columns) {
              kdb_value_t *pk_val = &values[table->pk_column];
              btree_key_t key = {0};
              bool key_valid = false;

              if (pk_val->type == KDB_TYPE_INTEGER) {
                key = btree_key_int(pk_val->v.i);
                key_valid = true;
              } else if (pk_val->type == KDB_TYPE_TEXT && pk_val->v.text.data) {
                key = btree_key_str(pk_val->v.text.data);
                key_valid = true;
              }

              if (key_valid) {
                btree_delete(table->pk_index, &key);
                btree_key_free(&key);
              }
            }

            for (int j = 0; j < table->num_columns; j++) {
              kdb_value_free(&values[j]);
            }
            free(values);
          }
        }

        table->row_count--;

        /* Save/Sync Metadata (row count) */
        page_t table_def_page = {0};
        table_def_page.header.page_id = 1 + (table - store->tables);
        table_def_page.header.type = PAGE_TABLE_DEF;
        memcpy(table_def_page.data, table, sizeof(table_t) - sizeof(btree_t *));

        fseek(store->file, table_def_page.header.page_id * KDB_PAGE_SIZE,
              SEEK_SET);
        fwrite(&table_def_page, sizeof(table_def_page), 1, store->file);
        fflush(store->file);

        pthread_mutex_unlock(&store->lock);
        return KDB_OK;
      }

      offset += 2 + 8 + size;
    }

    page_id = page.header.next_page;
  }

  pthread_mutex_unlock(&store->lock);
  return KDB_ERR_NO_TABLE;
}

/*
 * Cursor for iteration
 */
struct storage_cursor_s {
  storage_t *store;
  table_t *table;
  uint32_t page_id;
  uint32_t record_idx;
  size_t offset;
  page_t page;
};

storage_cursor_t *storage_cursor_create(storage_t *store, table_t *table) {
  if (!store || !table)
    return NULL;

  storage_cursor_t *cur = calloc(1, sizeof(storage_cursor_t));
  if (!cur)
    return NULL;

  cur->store = store;
  cur->table = table;
  cur->page_id = table->first_data_page;
  cur->record_idx = 0;
  cur->offset = 0;

  /* Load first page */
  pthread_mutex_lock(&store->lock);
  if (cur->page_id) {
    fseek(store->file, cur->page_id * KDB_PAGE_SIZE, SEEK_SET);
    fread(&cur->page, sizeof(page_t), 1, store->file);
  }
  pthread_mutex_unlock(&store->lock);

  return cur;
}

bool storage_cursor_next(storage_cursor_t *cursor, int64_t *rowid,
                         kdb_row_t *row) {
  if (!cursor || !cursor->page_id)
    return false;

  pthread_mutex_lock(&cursor->store->lock);

  while (cursor->page_id) {
    /* Skip deleted rows / find next valid row */
    while (cursor->record_idx < cursor->page.header.num_records) {
      uint16_t size;
      int64_t rid;
      memcpy(&size, cursor->page.data + cursor->offset, 2);
      memcpy(&rid, cursor->page.data + cursor->offset + 2, 8);

      if (rid != -1) {
        /* Valid row */
        if (rowid)
          *rowid = rid;

        if (row) {
          row->num_values = cursor->table->num_columns;
          row->values = calloc(row->num_values, sizeof(kdb_value_t));

          size_t pos = cursor->offset + 10;
          for (int j = 0; j < row->num_values; j++) {
            size_t read = deserialize_value(cursor->page.data + pos,
                                            size - (pos - cursor->offset - 10),
                                            &row->values[j]);
            if (read == 0)
              break;
            pos += read;
          }
        }

        /* Advance cursor */
        cursor->offset += 2 + 8 + size;
        cursor->record_idx++;

        pthread_mutex_unlock(&cursor->store->lock);
        return true;
      }

      cursor->offset += 2 + 8 + size;
      cursor->record_idx++;
    }

    /* Move to next page */
    cursor->page_id = cursor->page.header.next_page;
    if (cursor->page_id) {
      fseek(cursor->store->file, cursor->page_id * KDB_PAGE_SIZE, SEEK_SET);
      fread(&cursor->page, sizeof(page_t), 1, cursor->store->file);
      cursor->record_idx = 0;
      cursor->offset = 0;
    }
  }

  pthread_mutex_unlock(&cursor->store->lock);
  return false;
}

void storage_cursor_destroy(storage_cursor_t *cursor) { free(cursor); }

/*
 * Update a row (simplified - deletes and reinserts)
 */
kdb_error_t storage_update_row(storage_t *store, table_t *table, int64_t rowid,
                               const kdb_value_t *values) {
  (void)rowid;
  (void)values;
  (void)table;
  (void)store;
  /* For simplicity, update is delete + insert in the query layer */
  return KDB_OK;
}
/*
 * Rebuild indices from data pages
 */
kdb_error_t storage_rebuild_indices(storage_t *store) {
  if (!store)
    return KDB_ERR_IO;

  pthread_mutex_lock(&store->lock);
  printf("Rebuilding indices for %d tables...\n", store->num_tables);

  for (int i = 0; i < store->num_tables; i++) {
    table_t *t = &store->tables[i];
    if (t->pk_column < 0)
      continue; /* No PK to index */

    /* Recreate index */
    if (t->pk_index)
      btree_destroy(t->pk_index);
    t->pk_index = btree_create(true);

    printf("  Rebuilding index for table '%s'\n", t->name);

    if (t->row_count == 0)
      continue;

    /* Scan all data pages */
    uint32_t page_id = t->first_data_page;
    int count = 0;

    while (page_id) {
      page_t page;
      fseek(store->file, page_id * KDB_PAGE_SIZE, SEEK_SET);
      if (fread(&page, sizeof(page), 1, store->file) != 1)
        break;

      /* Scan rows in page */
      size_t offset = 0;
      for (uint32_t j = 0; j < page.header.num_records; j++) {
        uint16_t size;
        int64_t rid;
        memcpy(&size, page.data + offset, 2);
        memcpy(&rid, page.data + offset + 2, 8);

        if (rid != -1) { /* Skip deleted rows */
          /* We need to extract the PK value.
             This requires parsing the row data up to the PK column. */

          /* Parse values */
          size_t pos = offset + 10;
          kdb_value_t *values = calloc(t->num_columns, sizeof(kdb_value_t));
          if (values) {
            for (int k = 0; k < t->num_columns; k++) {
              size_t read = deserialize_value(
                  page.data + pos, size - (pos - offset - 10), &values[k]);
              if (read == 0)
                break;
              pos += read;
            }

            /* Insert into index */
            const kdb_value_t *pk_val = &values[t->pk_column];
            btree_key_t key = {0};
            bool key_valid = false;

            if (pk_val->type == KDB_TYPE_INTEGER) {
              key = btree_key_int(pk_val->v.i);
              key_valid = true;
            } else if (pk_val->type == KDB_TYPE_TEXT && pk_val->v.text.data) {
              key = btree_key_str(pk_val->v.text.data);
              key_valid = true;
            } else if (pk_val->type == KDB_TYPE_NULL &&
                       t->columns[t->pk_column].primary_key) {
              /* PK is auto-increment integer, use rowid if NULL */
              key = btree_key_int(rid);
              key_valid = true;
            }

            if (key_valid) {
              btree_insert(t->pk_index, key, rid);
              btree_key_free(&key);
              count++;
            }

            /* Cleanup */
            for (int k = 0; k < t->num_columns; k++) {
              kdb_value_free(&values[k]);
            }
            free(values);
          }
        }

        offset += 2 + 8 + size;
      }

      page_id = page.header.next_page;
    }
    printf("    Indexed %d rows for table '%s'\n", count, t->name);
  }

  pthread_mutex_unlock(&store->lock);
  return KDB_OK;
}

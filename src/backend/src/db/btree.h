/**
 * B-Tree Index Implementation
 * Order-64 B-tree for fast key-value lookups
 */
#ifndef KDB_BTREE_H
#define KDB_BTREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* B-tree order (max children per node) */
#define BTREE_ORDER 64
#define BTREE_MIN_KEYS ((BTREE_ORDER / 2) - 1)
#define BTREE_MAX_KEYS (BTREE_ORDER - 1)

/* Key type - supports integer and string keys */
typedef struct {
  enum { BTREE_KEY_INT, BTREE_KEY_STR } type;
  union {
    int64_t i;
    struct {
      char *data;
      size_t len;
    } s;
  } v;
} btree_key_t;

/* B-tree node */
typedef struct btree_node_s {
  btree_key_t keys[BTREE_MAX_KEYS];
  int64_t values[BTREE_MAX_KEYS]; /* Row IDs */
  struct btree_node_s *children[BTREE_ORDER];
  int num_keys;
  bool is_leaf;
} btree_node_t;

/* B-tree handle */
typedef struct {
  btree_node_t *root;
  size_t size; /* Number of entries */
  bool unique; /* Enforce unique keys */
  int (*compare)(const btree_key_t *, const btree_key_t *);
} btree_t;

/* Iterator for range scans */
typedef struct {
  btree_t *tree;
  btree_node_t *node;
  int index;
  btree_key_t *end_key; /* NULL for no limit */
} btree_iter_t;

/**
 * Create a new B-tree
 * @param unique  If true, reject duplicate keys
 * @return New B-tree or NULL on error
 */
btree_t *btree_create(bool unique);

/**
 * Destroy B-tree and free all memory
 */
void btree_destroy(btree_t *tree);

/**
 * Insert key-value pair
 * @param tree  B-tree handle
 * @param key   Key to insert
 * @param value Value (row ID) to associate
 * @return 0 on success, -1 on duplicate (if unique), -2 on memory error
 */
int btree_insert(btree_t *tree, btree_key_t key, int64_t value);

/**
 * Find value by key
 * @param tree  B-tree handle
 * @param key   Key to search
 * @param value Output value
 * @return true if found
 */
bool btree_find(btree_t *tree, const btree_key_t *key, int64_t *value);

/**
 * Delete key
 * @param tree B-tree handle
 * @param key  Key to delete
 * @return true if deleted
 */
bool btree_delete(btree_t *tree, const btree_key_t *key);

/**
 * Update value for existing key
 * @return true if key exists and was updated
 */
bool btree_update(btree_t *tree, const btree_key_t *key, int64_t new_value);

/**
 * Get number of entries
 */
size_t btree_size(btree_t *tree);

/* Iterator functions */
btree_iter_t *btree_iter_create(btree_t *tree, const btree_key_t *start_key);
bool btree_iter_next(btree_iter_t *iter, btree_key_t *key, int64_t *value);
void btree_iter_destroy(btree_iter_t *iter);

/* Key helper functions */
btree_key_t btree_key_int(int64_t i);
btree_key_t btree_key_str(const char *s);
void btree_key_free(btree_key_t *key);
int btree_key_compare(const btree_key_t *a, const btree_key_t *b);

#endif /* KDB_BTREE_H */

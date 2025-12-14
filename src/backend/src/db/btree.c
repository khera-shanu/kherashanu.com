/**
 * B-Tree Implementation
 * Order-64 B-tree for fast key-value lookups
 */
#include "btree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
static btree_node_t *node_create(bool is_leaf);
static void node_destroy(btree_node_t *node);
static int node_search_key(btree_node_t *node, const btree_key_t *key);
static void node_insert_nonfull(btree_node_t *node, btree_key_t key,
                                int64_t value);
static void node_split_child(btree_node_t *parent, int index,
                             btree_node_t *child);
static bool node_delete(btree_node_t *node, const btree_key_t *key,
                        bool *found);
static void key_copy(btree_key_t *dst, const btree_key_t *src);

/*
 * Key comparison
 */
int btree_key_compare(const btree_key_t *a, const btree_key_t *b) {
  if (a->type != b->type) {
    return (int)a->type - (int)b->type;
  }

  if (a->type == BTREE_KEY_INT) {
    if (a->v.i < b->v.i)
      return -1;
    if (a->v.i > b->v.i)
      return 1;
    return 0;
  } else {
    /* String comparison */
    size_t min_len = a->v.s.len < b->v.s.len ? a->v.s.len : b->v.s.len;
    int cmp = memcmp(a->v.s.data, b->v.s.data, min_len);
    if (cmp != 0)
      return cmp;
    if (a->v.s.len < b->v.s.len)
      return -1;
    if (a->v.s.len > b->v.s.len)
      return 1;
    return 0;
  }
}

/*
 * Key helper functions
 */
btree_key_t btree_key_int(int64_t i) {
  btree_key_t key = {0};
  key.type = BTREE_KEY_INT;
  key.v.i = i;
  return key;
}

btree_key_t btree_key_str(const char *s) {
  btree_key_t key = {0};
  key.type = BTREE_KEY_STR;
  size_t len = strlen(s);
  key.v.s.data = malloc(len + 1);
  if (key.v.s.data) {
    memcpy(key.v.s.data, s, len + 1);
    key.v.s.len = len;
  }
  return key;
}

void btree_key_free(btree_key_t *key) {
  if (key->type == BTREE_KEY_STR && key->v.s.data) {
    free(key->v.s.data);
    key->v.s.data = NULL;
    key->v.s.len = 0;
  }
}

static void key_copy(btree_key_t *dst, const btree_key_t *src) {
  dst->type = src->type;
  if (src->type == BTREE_KEY_INT) {
    dst->v.i = src->v.i;
  } else {
    dst->v.s.data = malloc(src->v.s.len + 1);
    if (dst->v.s.data) {
      memcpy(dst->v.s.data, src->v.s.data, src->v.s.len + 1);
      dst->v.s.len = src->v.s.len;
    }
  }
}

/*
 * Create new B-tree
 */
btree_t *btree_create(bool unique) {
  btree_t *tree = calloc(1, sizeof(btree_t));
  if (!tree)
    return NULL;

  tree->root = node_create(true);
  if (!tree->root) {
    free(tree);
    return NULL;
  }

  tree->size = 0;
  tree->unique = unique;
  tree->compare = btree_key_compare;

  return tree;
}

/*
 * Create new node
 */
static btree_node_t *node_create(bool is_leaf) {
  btree_node_t *node = calloc(1, sizeof(btree_node_t));
  if (!node)
    return NULL;
  node->is_leaf = is_leaf;
  node->num_keys = 0;
  return node;
}

/*
 * Destroy node and all children
 */
static void node_destroy(btree_node_t *node) {
  if (!node)
    return;

  /* Free string keys */
  for (int i = 0; i < node->num_keys; i++) {
    btree_key_free(&node->keys[i]);
  }

  /* Recursively destroy children */
  if (!node->is_leaf) {
    for (int i = 0; i <= node->num_keys; i++) {
      node_destroy(node->children[i]);
    }
  }

  free(node);
}

/*
 * Destroy entire B-tree
 */
void btree_destroy(btree_t *tree) {
  if (!tree)
    return;
  node_destroy(tree->root);
  free(tree);
}

/*
 * Search for key position in node
 * Returns index of first key >= search key
 */
static int node_search_key(btree_node_t *node, const btree_key_t *key) {
  int i = 0;
  while (i < node->num_keys && btree_key_compare(&node->keys[i], key) < 0) {
    i++;
  }
  return i;
}

/*
 * Find value by key
 */
bool btree_find(btree_t *tree, const btree_key_t *key, int64_t *value) {
  if (!tree || !tree->root)
    return false;

  btree_node_t *node = tree->root;

  while (node) {
    int i = node_search_key(node, key);

    if (i < node->num_keys && btree_key_compare(&node->keys[i], key) == 0) {
      if (value)
        *value = node->values[i];
      return true;
    }

    if (node->is_leaf) {
      return false;
    }

    node = node->children[i];
  }

  return false;
}

/*
 * Split a full child node
 */
static void node_split_child(btree_node_t *parent, int index,
                             btree_node_t *child) {
  int mid = BTREE_MAX_KEYS / 2;

  /* Create new node for right half */
  btree_node_t *new_node = node_create(child->is_leaf);
  if (!new_node)
    return;

  new_node->num_keys = child->num_keys - mid - 1;

  /* Copy right half of keys to new node */
  for (int i = 0; i < new_node->num_keys; i++) {
    key_copy(&new_node->keys[i], &child->keys[mid + 1 + i]);
    new_node->values[i] = child->values[mid + 1 + i];
    btree_key_free(&child->keys[mid + 1 + i]);
  }

  /* Copy children if not leaf */
  if (!child->is_leaf) {
    for (int i = 0; i <= new_node->num_keys; i++) {
      new_node->children[i] = child->children[mid + 1 + i];
      child->children[mid + 1 + i] = NULL;
    }
  }

  /* Move parent keys/children to make room */
  for (int i = parent->num_keys; i > index; i--) {
    key_copy(&parent->keys[i], &parent->keys[i - 1]);
    parent->values[i] = parent->values[i - 1];
    btree_key_free(&parent->keys[i - 1]);
  }
  for (int i = parent->num_keys + 1; i > index + 1; i--) {
    parent->children[i] = parent->children[i - 1];
  }

  /* Insert median key into parent */
  key_copy(&parent->keys[index], &child->keys[mid]);
  parent->values[index] = child->values[mid];
  btree_key_free(&child->keys[mid]);

  parent->children[index + 1] = new_node;
  parent->num_keys++;

  child->num_keys = mid;
}

/*
 * Insert into a non-full node
 */
static void node_insert_nonfull(btree_node_t *node, btree_key_t key,
                                int64_t value) {
  int i = node->num_keys - 1;

  if (node->is_leaf) {
    /* Find position and shift keys */
    while (i >= 0 && btree_key_compare(&node->keys[i], &key) > 0) {
      key_copy(&node->keys[i + 1], &node->keys[i]);
      node->values[i + 1] = node->values[i];
      btree_key_free(&node->keys[i]);
      i--;
    }

    key_copy(&node->keys[i + 1], &key);
    node->values[i + 1] = value;
    node->num_keys++;
  } else {
    /* Find child to recurse into */
    while (i >= 0 && btree_key_compare(&node->keys[i], &key) > 0) {
      i--;
    }
    i++;

    /* Split child if full */
    if (node->children[i]->num_keys == BTREE_MAX_KEYS) {
      node_split_child(node, i, node->children[i]);
      if (btree_key_compare(&node->keys[i], &key) < 0) {
        i++;
      }
    }

    node_insert_nonfull(node->children[i], key, value);
  }
}

/*
 * Insert key-value pair
 */
int btree_insert(btree_t *tree, btree_key_t key, int64_t value) {
  if (!tree)
    return -2;

  /* Check for duplicate if unique */
  if (tree->unique && btree_find(tree, &key, NULL)) {
    return -1;
  }

  btree_node_t *root = tree->root;

  /* If root is full, create new root */
  if (root->num_keys == BTREE_MAX_KEYS) {
    btree_node_t *new_root = node_create(false);
    if (!new_root)
      return -2;

    new_root->children[0] = root;
    tree->root = new_root;

    node_split_child(new_root, 0, root);

    /* Determine which child to insert into */
    int i = 0;
    if (btree_key_compare(&new_root->keys[0], &key) < 0) {
      i = 1;
    }
    node_insert_nonfull(new_root->children[i], key, value);
  } else {
    node_insert_nonfull(root, key, value);
  }

  tree->size++;
  return 0;
}

/*
 * Update value for existing key
 */
bool btree_update(btree_t *tree, const btree_key_t *key, int64_t new_value) {
  if (!tree || !tree->root)
    return false;

  btree_node_t *node = tree->root;

  while (node) {
    int i = node_search_key(node, key);

    if (i < node->num_keys && btree_key_compare(&node->keys[i], key) == 0) {
      node->values[i] = new_value;
      return true;
    }

    if (node->is_leaf) {
      return false;
    }

    node = node->children[i];
  }

  return false;
}

/*
 * Get predecessor key (rightmost in left subtree)
 */
static void get_predecessor(btree_node_t *node, int idx, btree_key_t *key,
                            int64_t *value) {
  btree_node_t *cur = node->children[idx];
  while (!cur->is_leaf) {
    cur = cur->children[cur->num_keys];
  }
  key_copy(key, &cur->keys[cur->num_keys - 1]);
  *value = cur->values[cur->num_keys - 1];
}

/*
 * Get successor key (leftmost in right subtree)
 */
static void get_successor(btree_node_t *node, int idx, btree_key_t *key,
                          int64_t *value) {
  btree_node_t *cur = node->children[idx + 1];
  while (!cur->is_leaf) {
    cur = cur->children[0];
  }
  key_copy(key, &cur->keys[0]);
  *value = cur->values[0];
}

/*
 * Merge child at idx with child at idx+1
 */
static void merge_children(btree_node_t *node, int idx) {
  btree_node_t *left = node->children[idx];
  btree_node_t *right = node->children[idx + 1];

  /* Copy parent key to left child */
  key_copy(&left->keys[left->num_keys], &node->keys[idx]);
  left->values[left->num_keys] = node->values[idx];

  /* Copy all keys from right to left */
  for (int i = 0; i < right->num_keys; i++) {
    key_copy(&left->keys[left->num_keys + 1 + i], &right->keys[i]);
    left->values[left->num_keys + 1 + i] = right->values[i];
  }

  /* Copy children from right to left */
  if (!left->is_leaf) {
    for (int i = 0; i <= right->num_keys; i++) {
      left->children[left->num_keys + 1 + i] = right->children[i];
    }
  }

  left->num_keys += right->num_keys + 1;

  /* Shift parent keys and children */
  btree_key_free(&node->keys[idx]);
  for (int i = idx; i < node->num_keys - 1; i++) {
    key_copy(&node->keys[i], &node->keys[i + 1]);
    node->values[i] = node->values[i + 1];
    btree_key_free(&node->keys[i + 1]);
  }
  for (int i = idx + 1; i < node->num_keys; i++) {
    node->children[i] = node->children[i + 1];
  }
  node->num_keys--;

  /* Free right keys and node */
  for (int i = 0; i < right->num_keys; i++) {
    btree_key_free(&right->keys[i]);
  }
  free(right);
}

/*
 * Fill child that has fewer than minimum keys
 */
static void fill_child(btree_node_t *node, int idx) {
  /* Try borrowing from left sibling */
  if (idx > 0 && node->children[idx - 1]->num_keys > BTREE_MIN_KEYS) {
    btree_node_t *child = node->children[idx];
    btree_node_t *sibling = node->children[idx - 1];

    /* Shift child keys right */
    for (int i = child->num_keys - 1; i >= 0; i--) {
      key_copy(&child->keys[i + 1], &child->keys[i]);
      child->values[i + 1] = child->values[i];
      btree_key_free(&child->keys[i]);
    }
    if (!child->is_leaf) {
      for (int i = child->num_keys; i >= 0; i--) {
        child->children[i + 1] = child->children[i];
      }
    }

    /* Move parent key to child */
    key_copy(&child->keys[0], &node->keys[idx - 1]);
    child->values[0] = node->values[idx - 1];

    /* Move sibling's last key to parent */
    btree_key_free(&node->keys[idx - 1]);
    key_copy(&node->keys[idx - 1], &sibling->keys[sibling->num_keys - 1]);
    node->values[idx - 1] = sibling->values[sibling->num_keys - 1];

    if (!child->is_leaf) {
      child->children[0] = sibling->children[sibling->num_keys];
    }

    btree_key_free(&sibling->keys[sibling->num_keys - 1]);
    sibling->num_keys--;
    child->num_keys++;
  }
  /* Try borrowing from right sibling */
  else if (idx < node->num_keys &&
           node->children[idx + 1]->num_keys > BTREE_MIN_KEYS) {
    btree_node_t *child = node->children[idx];
    btree_node_t *sibling = node->children[idx + 1];

    /* Move parent key to child */
    key_copy(&child->keys[child->num_keys], &node->keys[idx]);
    child->values[child->num_keys] = node->values[idx];

    if (!child->is_leaf) {
      child->children[child->num_keys + 1] = sibling->children[0];
    }

    child->num_keys++;

    /* Move sibling's first key to parent */
    btree_key_free(&node->keys[idx]);
    key_copy(&node->keys[idx], &sibling->keys[0]);
    node->values[idx] = sibling->values[0];

    /* Shift sibling keys left */
    btree_key_free(&sibling->keys[0]);
    for (int i = 0; i < sibling->num_keys - 1; i++) {
      key_copy(&sibling->keys[i], &sibling->keys[i + 1]);
      sibling->values[i] = sibling->values[i + 1];
      btree_key_free(&sibling->keys[i + 1]);
    }
    if (!sibling->is_leaf) {
      for (int i = 0; i < sibling->num_keys; i++) {
        sibling->children[i] = sibling->children[i + 1];
      }
    }

    sibling->num_keys--;
  }
  /* Merge with a sibling */
  else {
    if (idx < node->num_keys) {
      merge_children(node, idx);
    } else {
      merge_children(node, idx - 1);
    }
  }
}

/*
 * Delete key from node (recursive)
 */
static bool node_delete(btree_node_t *node, const btree_key_t *key,
                        bool *found) {
  int idx = node_search_key(node, key);

  if (idx < node->num_keys && btree_key_compare(&node->keys[idx], key) == 0) {
    *found = true;

    if (node->is_leaf) {
      /* Simple deletion from leaf */
      btree_key_free(&node->keys[idx]);
      for (int i = idx; i < node->num_keys - 1; i++) {
        key_copy(&node->keys[i], &node->keys[i + 1]);
        node->values[i] = node->values[i + 1];
        btree_key_free(&node->keys[i + 1]);
      }
      node->num_keys--;
    } else {
      /* Key is in internal node */
      if (node->children[idx]->num_keys > BTREE_MIN_KEYS) {
        /* Replace with predecessor */
        btree_key_t pred_key = {0};
        int64_t pred_val;
        get_predecessor(node, idx, &pred_key, &pred_val);
        btree_key_free(&node->keys[idx]);
        key_copy(&node->keys[idx], &pred_key);
        node->values[idx] = pred_val;
        node_delete(node->children[idx], &pred_key, found);
        btree_key_free(&pred_key);
      } else if (node->children[idx + 1]->num_keys > BTREE_MIN_KEYS) {
        /* Replace with successor */
        btree_key_t succ_key = {0};
        int64_t succ_val;
        get_successor(node, idx, &succ_key, &succ_val);
        btree_key_free(&node->keys[idx]);
        key_copy(&node->keys[idx], &succ_key);
        node->values[idx] = succ_val;
        node_delete(node->children[idx + 1], &succ_key, found);
        btree_key_free(&succ_key);
      } else {
        /* Merge children and delete from merged node */
        merge_children(node, idx);
        node_delete(node->children[idx], key, found);
      }
    }
  } else {
    /* Key not in this node */
    if (node->is_leaf) {
      *found = false;
      return false;
    }

    /* Ensure child has enough keys */
    bool last_child = (idx == node->num_keys);
    if (node->children[idx]->num_keys <= BTREE_MIN_KEYS) {
      fill_child(node, idx);
    }

    /* After fill, idx may have changed due to merge */
    if (last_child && idx > node->num_keys) {
      node_delete(node->children[idx - 1], key, found);
    } else {
      node_delete(node->children[idx], key, found);
    }
  }

  return *found;
}

/*
 * Delete key from tree
 */
bool btree_delete(btree_t *tree, const btree_key_t *key) {
  if (!tree || !tree->root)
    return false;

  bool found = false;
  node_delete(tree->root, key, &found);

  if (found) {
    tree->size--;

    /* If root has no keys but has a child, make the child the new root */
    if (tree->root->num_keys == 0 && !tree->root->is_leaf) {
      btree_node_t *old_root = tree->root;
      tree->root = tree->root->children[0];
      free(old_root);
    }
  }

  return found;
}

/*
 * Get tree size
 */
size_t btree_size(btree_t *tree) { return tree ? tree->size : 0; }

/*
 * Iterator implementation
 */
typedef struct iter_stack_s {
  btree_node_t *node;
  int index;
  struct iter_stack_s *next;
} iter_stack_t;

struct btree_iter_internal {
  btree_t *tree;
  iter_stack_t *stack;
  btree_key_t *end_key;
  bool done;
};

static void push_leftmost(btree_iter_t *iter, btree_node_t *node) {
  struct btree_iter_internal *it = (struct btree_iter_internal *)iter;
  while (node) {
    iter_stack_t *frame = malloc(sizeof(iter_stack_t));
    frame->node = node;
    frame->index = 0;
    frame->next = it->stack;
    it->stack = frame;

    if (node->is_leaf)
      break;
    node = node->children[0];
  }
}

btree_iter_t *btree_iter_create(btree_t *tree, const btree_key_t *start_key) {
  if (!tree)
    return NULL;

  struct btree_iter_internal *iter =
      calloc(1, sizeof(struct btree_iter_internal));
  if (!iter)
    return NULL;

  iter->tree = tree;
  iter->done = false;

  if (start_key) {
    /* Find starting position */
    btree_node_t *node = tree->root;
    while (node) {
      int i = node_search_key(node, start_key);
      iter_stack_t *frame = malloc(sizeof(iter_stack_t));
      frame->node = node;
      frame->index = i;
      frame->next = iter->stack;
      iter->stack = frame;

      if (node->is_leaf)
        break;
      if (i < node->num_keys &&
          btree_key_compare(&node->keys[i], start_key) == 0) {
        break;
      }
      node = node->children[i];
    }
  } else {
    push_leftmost((btree_iter_t *)iter, tree->root);
  }

  return (btree_iter_t *)iter;
}

bool btree_iter_next(btree_iter_t *iter, btree_key_t *key, int64_t *value) {
  struct btree_iter_internal *it = (struct btree_iter_internal *)iter;
  if (!it || it->done || !it->stack)
    return false;

  while (it->stack) {
    iter_stack_t *frame = it->stack;

    if (frame->node->is_leaf) {
      if (frame->index < frame->node->num_keys) {
        if (key)
          key_copy(key, &frame->node->keys[frame->index]);
        if (value)
          *value = frame->node->values[frame->index];
        frame->index++;
        return true;
      }
      /* Pop and go back to parent */
      it->stack = frame->next;
      free(frame);
    } else {
      if (frame->index < frame->node->num_keys) {
        /* Return current key */
        if (key)
          key_copy(key, &frame->node->keys[frame->index]);
        if (value)
          *value = frame->node->values[frame->index];

        /* Push right child for next iteration */
        frame->index++;
        push_leftmost(iter, frame->node->children[frame->index]);
        return true;
      }
      /* Pop this frame */
      it->stack = frame->next;
      free(frame);
    }
  }

  it->done = true;
  return false;
}

void btree_iter_destroy(btree_iter_t *iter) {
  struct btree_iter_internal *it = (struct btree_iter_internal *)iter;
  if (!it)
    return;

  while (it->stack) {
    iter_stack_t *frame = it->stack;
    it->stack = frame->next;
    free(frame);
  }

  if (it->end_key) {
    btree_key_free(it->end_key);
    free(it->end_key);
  }

  free(it);
}

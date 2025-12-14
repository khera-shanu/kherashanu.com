/**
 * Kherashanu JSON Parser - Lightweight JSON parser/serializer
 * Zero external dependencies, designed for high performance.
 */
#ifndef KFW_JSON_H
#define KFW_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* JSON value types */
typedef enum {
  JSON_NULL = 0,
  JSON_BOOL,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT
} json_type_t;

/* Forward declaration */
typedef struct json_value json_value_t;

/* JSON object key-value pair */
typedef struct {
  char *key;
  json_value_t *value;
} json_pair_t;

/* JSON array */
typedef struct {
  json_value_t **items;
  size_t count;
  size_t capacity;
} json_array_t;

/* JSON object */
typedef struct {
  json_pair_t *pairs;
  size_t count;
  size_t capacity;
} json_object_t;

/* JSON value */
struct json_value {
  json_type_t type;
  union {
    bool boolean;
    double number;
    char *string;
    json_array_t array;
    json_object_t object;
  } v;
};

/* Error codes */
typedef enum {
  JSON_OK = 0,
  JSON_ERR_NOMEM,
  JSON_ERR_SYNTAX,
  JSON_ERR_EOF,
  JSON_ERR_INVALID_CHAR,
  JSON_ERR_INVALID_ESCAPE,
  JSON_ERR_INVALID_NUMBER,
  JSON_ERR_NESTING_TOO_DEEP,
  JSON_ERR_DUPLICATE_KEY
} json_error_t;

/* Parser context for error reporting */
typedef struct {
  json_error_t error;
  size_t error_pos;
  char error_msg[128];
} json_ctx_t;

/**
 * Parse JSON string into value tree
 * @param input   JSON string (null-terminated)
 * @param ctx     Optional context for error info (can be NULL)
 * @return        Root JSON value, or NULL on error
 */
json_value_t *json_parse(const char *input, json_ctx_t *ctx);

/**
 * Parse JSON string with length (not null-terminated)
 * @param input   JSON string
 * @param len     String length
 * @param ctx     Optional context for error info (can be NULL)
 * @return        Root JSON value, or NULL on error
 */
json_value_t *json_parse_n(const char *input, size_t len, json_ctx_t *ctx);

/**
 * Serialize JSON value to string
 * @param value   JSON value tree
 * @param pretty  If true, format with indentation
 * @return        Allocated string (caller must free), or NULL on error
 */
char *json_stringify(const json_value_t *value, bool pretty);

/**
 * Free JSON value tree
 */
void json_free(json_value_t *value);

/* --- Value constructors --- */

json_value_t *json_null(void);
json_value_t *json_bool(bool val);
json_value_t *json_number(double val);
json_value_t *json_string(const char *val);
json_value_t *json_string_n(const char *val, size_t len);
json_value_t *json_array_new(void);
json_value_t *json_object_new(void);

/* --- Array operations --- */

/**
 * Append value to array
 * @return true on success
 */
bool json_array_push(json_value_t *arr, json_value_t *val);

/**
 * Get array element by index
 * @return Element or NULL if out of bounds
 */
json_value_t *json_array_get(const json_value_t *arr, size_t index);

/**
 * Get array length
 */
size_t json_array_len(const json_value_t *arr);

/* --- Object operations --- */

/**
 * Set object key-value pair (overwrites existing)
 * @return true on success
 */
bool json_object_set(json_value_t *obj, const char *key, json_value_t *val);

/**
 * Get object value by key
 * @return Value or NULL if key not found
 */
json_value_t *json_object_get(const json_value_t *obj, const char *key);

/**
 * Check if object has key
 */
bool json_object_has(const json_value_t *obj, const char *key);

/**
 * Get number of keys in object
 */
size_t json_object_len(const json_value_t *obj);

/* --- Type checking --- */

static inline bool json_is_null(const json_value_t *v) {
  return v && v->type == JSON_NULL;
}
static inline bool json_is_bool(const json_value_t *v) {
  return v && v->type == JSON_BOOL;
}
static inline bool json_is_number(const json_value_t *v) {
  return v && v->type == JSON_NUMBER;
}
static inline bool json_is_string(const json_value_t *v) {
  return v && v->type == JSON_STRING;
}
static inline bool json_is_array(const json_value_t *v) {
  return v && v->type == JSON_ARRAY;
}
static inline bool json_is_object(const json_value_t *v) {
  return v && v->type == JSON_OBJECT;
}

/* --- Value getters (no type checking, caller should verify type) --- */

static inline bool json_get_bool(const json_value_t *v) { return v->v.boolean; }
static inline double json_get_number(const json_value_t *v) {
  return v->v.number;
}
static inline const char *json_get_string(const json_value_t *v) {
  return v->v.string;
}

/* --- Integer helpers --- */

static inline int64_t json_get_int(const json_value_t *v) {
  return (int64_t)v->v.number;
}

static inline json_value_t *json_int(int64_t val) {
  return json_number((double)val);
}

#endif /* KFW_JSON_H */

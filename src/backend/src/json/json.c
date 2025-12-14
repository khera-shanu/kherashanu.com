/**
 * Kherashanu JSON Parser - Implementation
 * Recursive descent parser with tokenizer
 */
#include "json.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NESTING_DEPTH 128
#define INITIAL_CAPACITY 8

/* ========== Tokenizer ========== */

typedef enum {
  TOK_EOF = 0,
  TOK_LBRACE,   /* { */
  TOK_RBRACE,   /* } */
  TOK_LBRACKET, /* [ */
  TOK_RBRACKET, /* ] */
  TOK_COLON,    /* : */
  TOK_COMMA,    /* , */
  TOK_STRING,
  TOK_NUMBER,
  TOK_TRUE,
  TOK_FALSE,
  TOK_NULL,
  TOK_ERROR
} token_type_t;

typedef struct {
  token_type_t type;
  const char *start;
  size_t len;
  double num_val; /* For numbers */
} token_t;

typedef struct {
  const char *input;
  size_t len;
  size_t pos;
  int depth;
  json_ctx_t *ctx;
  token_t current;
} parser_t;

static void set_error(parser_t *p, json_error_t err, const char *msg) {
  if (p->ctx) {
    p->ctx->error = err;
    p->ctx->error_pos = p->pos;
    snprintf(p->ctx->error_msg, sizeof(p->ctx->error_msg), "%s", msg);
  }
}

static void skip_whitespace(parser_t *p) {
  while (p->pos < p->len) {
    char c = p->input[p->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      p->pos++;
    } else {
      break;
    }
  }
}

static bool parse_string_token(parser_t *p) {
  if (p->input[p->pos] != '"')
    return false;
  p->pos++; /* Skip opening quote */

  p->current.start = p->input + p->pos;
  size_t start = p->pos;

  while (p->pos < p->len) {
    char c = p->input[p->pos];
    if (c == '"') {
      p->current.len = p->pos - start;
      p->current.type = TOK_STRING;
      p->pos++; /* Skip closing quote */
      return true;
    }
    if (c == '\\') {
      p->pos++;
      if (p->pos >= p->len) {
        set_error(p, JSON_ERR_INVALID_ESCAPE, "Unterminated escape sequence");
        return false;
      }
      /* Skip escaped char */
      char esc = p->input[p->pos];
      if (esc == 'u') {
        /* Unicode escape: \uXXXX */
        p->pos++;
        for (int i = 0; i < 4; i++) {
          if (p->pos >= p->len || !isxdigit((unsigned char)p->input[p->pos])) {
            set_error(p, JSON_ERR_INVALID_ESCAPE, "Invalid unicode escape");
            return false;
          }
          p->pos++;
        }
        continue;
      }
      p->pos++;
    } else if ((unsigned char)c < 0x20) {
      set_error(p, JSON_ERR_INVALID_CHAR, "Control character in string");
      return false;
    } else {
      p->pos++;
    }
  }

  set_error(p, JSON_ERR_SYNTAX, "Unterminated string");
  return false;
}

static bool parse_number_token(parser_t *p) {
  const char *start = p->input + p->pos;
  size_t start_pos = p->pos;

  /* Optional minus */
  if (p->pos < p->len && p->input[p->pos] == '-') {
    p->pos++;
  }

  /* Integer part */
  if (p->pos >= p->len || !isdigit((unsigned char)p->input[p->pos])) {
    set_error(p, JSON_ERR_INVALID_NUMBER, "Invalid number");
    p->pos = start_pos;
    return false;
  }

  if (p->input[p->pos] == '0') {
    p->pos++;
  } else {
    while (p->pos < p->len && isdigit((unsigned char)p->input[p->pos])) {
      p->pos++;
    }
  }

  /* Fractional part */
  if (p->pos < p->len && p->input[p->pos] == '.') {
    p->pos++;
    if (p->pos >= p->len || !isdigit((unsigned char)p->input[p->pos])) {
      set_error(p, JSON_ERR_INVALID_NUMBER, "Invalid fractional part");
      p->pos = start_pos;
      return false;
    }
    while (p->pos < p->len && isdigit((unsigned char)p->input[p->pos])) {
      p->pos++;
    }
  }

  /* Exponent part */
  if (p->pos < p->len && (p->input[p->pos] == 'e' || p->input[p->pos] == 'E')) {
    p->pos++;
    if (p->pos < p->len &&
        (p->input[p->pos] == '+' || p->input[p->pos] == '-')) {
      p->pos++;
    }
    if (p->pos >= p->len || !isdigit((unsigned char)p->input[p->pos])) {
      set_error(p, JSON_ERR_INVALID_NUMBER, "Invalid exponent");
      p->pos = start_pos;
      return false;
    }
    while (p->pos < p->len && isdigit((unsigned char)p->input[p->pos])) {
      p->pos++;
    }
  }

  p->current.type = TOK_NUMBER;
  p->current.start = start;
  p->current.len = p->pos - start_pos;

  /* Parse the number value */
  char *endptr;
  char buf[64];
  size_t copy_len = p->current.len < 63 ? p->current.len : 63;
  memcpy(buf, start, copy_len);
  buf[copy_len] = '\0';
  p->current.num_val = strtod(buf, &endptr);

  return true;
}

static bool match_keyword(parser_t *p, const char *kw, size_t len,
                          token_type_t type) {
  if (p->pos + len <= p->len && memcmp(p->input + p->pos, kw, len) == 0) {
    /* Make sure it's not followed by alphanumeric */
    if (p->pos + len < p->len &&
        isalnum((unsigned char)p->input[p->pos + len])) {
      return false;
    }
    p->current.type = type;
    p->current.start = p->input + p->pos;
    p->current.len = len;
    p->pos += len;
    return true;
  }
  return false;
}

static void next_token(parser_t *p) {
  skip_whitespace(p);

  if (p->pos >= p->len) {
    p->current.type = TOK_EOF;
    return;
  }

  char c = p->input[p->pos];

  switch (c) {
  case '{':
    p->current.type = TOK_LBRACE;
    p->pos++;
    return;
  case '}':
    p->current.type = TOK_RBRACE;
    p->pos++;
    return;
  case '[':
    p->current.type = TOK_LBRACKET;
    p->pos++;
    return;
  case ']':
    p->current.type = TOK_RBRACKET;
    p->pos++;
    return;
  case ':':
    p->current.type = TOK_COLON;
    p->pos++;
    return;
  case ',':
    p->current.type = TOK_COMMA;
    p->pos++;
    return;
  case '"':
    if (!parse_string_token(p)) {
      p->current.type = TOK_ERROR;
    }
    return;
  case 't':
    if (match_keyword(p, "true", 4, TOK_TRUE))
      return;
    break;
  case 'f':
    if (match_keyword(p, "false", 5, TOK_FALSE))
      return;
    break;
  case 'n':
    if (match_keyword(p, "null", 4, TOK_NULL))
      return;
    break;
  default:
    if (c == '-' || isdigit((unsigned char)c)) {
      if (parse_number_token(p))
        return;
    }
    break;
  }

  set_error(p, JSON_ERR_INVALID_CHAR, "Unexpected character");
  p->current.type = TOK_ERROR;
}

/* ========== Parser ========== */

static json_value_t *parse_value(parser_t *p);

/* Unescape a JSON string */
static char *unescape_string(const char *src, size_t len) {
  char *dst = malloc(len + 1);
  if (!dst)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if (src[i] == '\\' && i + 1 < len) {
      i++;
      switch (src[i]) {
      case '"':
        dst[j++] = '"';
        break;
      case '\\':
        dst[j++] = '\\';
        break;
      case '/':
        dst[j++] = '/';
        break;
      case 'b':
        dst[j++] = '\b';
        break;
      case 'f':
        dst[j++] = '\f';
        break;
      case 'n':
        dst[j++] = '\n';
        break;
      case 'r':
        dst[j++] = '\r';
        break;
      case 't':
        dst[j++] = '\t';
        break;
      case 'u': {
        /* Unicode escape \uXXXX - simplified: just convert to UTF-8 or skip */
        if (i + 4 < len) {
          char hex[5] = {src[i + 1], src[i + 2], src[i + 3], src[i + 4], '\0'};
          unsigned int codepoint = (unsigned int)strtoul(hex, NULL, 16);
          i += 4;
          /* Simple UTF-8 encoding */
          if (codepoint < 0x80) {
            dst[j++] = (char)codepoint;
          } else if (codepoint < 0x800) {
            dst[j++] = (char)(0xC0 | (codepoint >> 6));
            dst[j++] = (char)(0x80 | (codepoint & 0x3F));
          } else {
            dst[j++] = (char)(0xE0 | (codepoint >> 12));
            dst[j++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            dst[j++] = (char)(0x80 | (codepoint & 0x3F));
          }
        }
        break;
      }
      default:
        dst[j++] = src[i];
        break;
      }
    } else {
      dst[j++] = src[i];
    }
  }
  dst[j] = '\0';
  return dst;
}

static json_value_t *parse_array(parser_t *p) {
  p->depth++;
  if (p->depth > MAX_NESTING_DEPTH) {
    set_error(p, JSON_ERR_NESTING_TOO_DEEP, "Nesting too deep");
    return NULL;
  }

  json_value_t *arr = json_array_new();
  if (!arr)
    return NULL;

  next_token(p); /* Skip [ */

  if (p->current.type == TOK_RBRACKET) {
    p->depth--;
    next_token(p);
    return arr;
  }

  while (1) {
    json_value_t *val = parse_value(p);
    if (!val) {
      json_free(arr);
      return NULL;
    }

    if (!json_array_push(arr, val)) {
      json_free(val);
      json_free(arr);
      return NULL;
    }

    if (p->current.type == TOK_RBRACKET) {
      next_token(p);
      break;
    }

    if (p->current.type != TOK_COMMA) {
      set_error(p, JSON_ERR_SYNTAX, "Expected ',' or ']'");
      json_free(arr);
      return NULL;
    }
    next_token(p); /* Skip , */
  }

  p->depth--;
  return arr;
}

static json_value_t *parse_object(parser_t *p) {
  p->depth++;
  if (p->depth > MAX_NESTING_DEPTH) {
    set_error(p, JSON_ERR_NESTING_TOO_DEEP, "Nesting too deep");
    return NULL;
  }

  json_value_t *obj = json_object_new();
  if (!obj)
    return NULL;

  next_token(p); /* Skip { */

  if (p->current.type == TOK_RBRACE) {
    p->depth--;
    next_token(p);
    return obj;
  }

  while (1) {
    if (p->current.type != TOK_STRING) {
      set_error(p, JSON_ERR_SYNTAX, "Expected string key");
      json_free(obj);
      return NULL;
    }

    char *key = unescape_string(p->current.start, p->current.len);
    if (!key) {
      json_free(obj);
      return NULL;
    }

    next_token(p);

    if (p->current.type != TOK_COLON) {
      set_error(p, JSON_ERR_SYNTAX, "Expected ':'");
      free(key);
      json_free(obj);
      return NULL;
    }
    next_token(p);

    json_value_t *val = parse_value(p);
    if (!val) {
      free(key);
      json_free(obj);
      return NULL;
    }

    if (!json_object_set(obj, key, val)) {
      free(key);
      json_free(val);
      json_free(obj);
      return NULL;
    }
    free(key);

    if (p->current.type == TOK_RBRACE) {
      next_token(p);
      break;
    }

    if (p->current.type != TOK_COMMA) {
      set_error(p, JSON_ERR_SYNTAX, "Expected ',' or '}'");
      json_free(obj);
      return NULL;
    }
    next_token(p);
  }

  p->depth--;
  return obj;
}

static json_value_t *parse_value(parser_t *p) {
  switch (p->current.type) {
  case TOK_NULL:
    next_token(p);
    return json_null();

  case TOK_TRUE:
    next_token(p);
    return json_bool(true);

  case TOK_FALSE:
    next_token(p);
    return json_bool(false);

  case TOK_NUMBER: {
    json_value_t *v = json_number(p->current.num_val);
    next_token(p);
    return v;
  }

  case TOK_STRING: {
    char *str = unescape_string(p->current.start, p->current.len);
    if (!str)
      return NULL;
    json_value_t *v = malloc(sizeof(json_value_t));
    if (!v) {
      free(str);
      return NULL;
    }
    v->type = JSON_STRING;
    v->v.string = str;
    next_token(p);
    return v;
  }

  case TOK_LBRACKET:
    return parse_array(p);

  case TOK_LBRACE:
    return parse_object(p);

  case TOK_EOF:
    set_error(p, JSON_ERR_EOF, "Unexpected end of input");
    return NULL;

  default:
    set_error(p, JSON_ERR_SYNTAX, "Unexpected token");
    return NULL;
  }
}

/* ========== Public API ========== */

json_value_t *json_parse(const char *input, json_ctx_t *ctx) {
  return json_parse_n(input, strlen(input), ctx);
}

json_value_t *json_parse_n(const char *input, size_t len, json_ctx_t *ctx) {
  if (ctx) {
    memset(ctx, 0, sizeof(json_ctx_t));
  }

  parser_t p = {.input = input, .len = len, .pos = 0, .depth = 0, .ctx = ctx};

  next_token(&p);
  json_value_t *result = parse_value(&p);

  if (result && p.current.type != TOK_EOF) {
    skip_whitespace(&p);
    if (p.pos < p.len) {
      set_error(&p, JSON_ERR_SYNTAX, "Trailing content after JSON");
      json_free(result);
      return NULL;
    }
  }

  return result;
}

/* ========== Stringify ========== */

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
  bool pretty;
  int indent;
} stringifier_t;

static bool sb_grow(stringifier_t *s, size_t needed) {
  if (s->len + needed >= s->cap) {
    size_t new_cap = s->cap * 2;
    if (new_cap < s->len + needed + 1)
      new_cap = s->len + needed + 256;
    char *new_buf = realloc(s->buf, new_cap);
    if (!new_buf)
      return false;
    s->buf = new_buf;
    s->cap = new_cap;
  }
  return true;
}

static bool sb_append(stringifier_t *s, const char *str, size_t len) {
  if (!sb_grow(s, len))
    return false;
  memcpy(s->buf + s->len, str, len);
  s->len += len;
  s->buf[s->len] = '\0';
  return true;
}

static bool sb_append_char(stringifier_t *s, char c) {
  return sb_append(s, &c, 1);
}

static bool sb_append_str(stringifier_t *s, const char *str) {
  return sb_append(s, str, strlen(str));
}

static bool sb_indent(stringifier_t *s) {
  if (!s->pretty)
    return true;
  for (int i = 0; i < s->indent * 2; i++) {
    if (!sb_append_char(s, ' '))
      return false;
  }
  return true;
}

static bool sb_newline(stringifier_t *s) {
  if (!s->pretty)
    return true;
  return sb_append_char(s, '\n');
}

static bool stringify_value(stringifier_t *s, const json_value_t *v);

static bool stringify_string(stringifier_t *s, const char *str) {
  if (!sb_append_char(s, '"'))
    return false;

  for (const char *p = str; *p; p++) {
    switch (*p) {
    case '"':
      if (!sb_append_str(s, "\\\""))
        return false;
      break;
    case '\\':
      if (!sb_append_str(s, "\\\\"))
        return false;
      break;
    case '\b':
      if (!sb_append_str(s, "\\b"))
        return false;
      break;
    case '\f':
      if (!sb_append_str(s, "\\f"))
        return false;
      break;
    case '\n':
      if (!sb_append_str(s, "\\n"))
        return false;
      break;
    case '\r':
      if (!sb_append_str(s, "\\r"))
        return false;
      break;
    case '\t':
      if (!sb_append_str(s, "\\t"))
        return false;
      break;
    default:
      if ((unsigned char)*p < 0x20) {
        char esc[8];
        snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)*p);
        if (!sb_append_str(s, esc))
          return false;
      } else {
        if (!sb_append_char(s, *p))
          return false;
      }
      break;
    }
  }

  return sb_append_char(s, '"');
}

static bool stringify_array(stringifier_t *s, const json_value_t *v) {
  if (!sb_append_char(s, '['))
    return false;

  if (v->v.array.count > 0) {
    if (!sb_newline(s))
      return false;
    s->indent++;

    for (size_t i = 0; i < v->v.array.count; i++) {
      if (i > 0) {
        if (!sb_append_char(s, ','))
          return false;
        if (!sb_newline(s))
          return false;
      }
      if (!sb_indent(s))
        return false;
      if (!stringify_value(s, v->v.array.items[i]))
        return false;
    }

    s->indent--;
    if (!sb_newline(s))
      return false;
    if (!sb_indent(s))
      return false;
  }

  return sb_append_char(s, ']');
}

static bool stringify_object(stringifier_t *s, const json_value_t *v) {
  if (!sb_append_char(s, '{'))
    return false;

  if (v->v.object.count > 0) {
    if (!sb_newline(s))
      return false;
    s->indent++;

    for (size_t i = 0; i < v->v.object.count; i++) {
      if (i > 0) {
        if (!sb_append_char(s, ','))
          return false;
        if (!sb_newline(s))
          return false;
      }
      if (!sb_indent(s))
        return false;
      if (!stringify_string(s, v->v.object.pairs[i].key))
        return false;
      if (!sb_append_char(s, ':'))
        return false;
      if (s->pretty) {
        if (!sb_append_char(s, ' '))
          return false;
      }
      if (!stringify_value(s, v->v.object.pairs[i].value))
        return false;
    }

    s->indent--;
    if (!sb_newline(s))
      return false;
    if (!sb_indent(s))
      return false;
  }

  return sb_append_char(s, '}');
}

static bool stringify_value(stringifier_t *s, const json_value_t *v) {
  if (!v) {
    return sb_append_str(s, "null");
  }

  switch (v->type) {
  case JSON_NULL:
    return sb_append_str(s, "null");

  case JSON_BOOL:
    return sb_append_str(s, v->v.boolean ? "true" : "false");

  case JSON_NUMBER: {
    char buf[64];
    double num = v->v.number;
    /* Check if it's an integer */
    if (num == (double)(int64_t)num && num >= -9007199254740992.0 &&
        num <= 9007199254740992.0) {
      snprintf(buf, sizeof(buf), "%lld", (long long)(int64_t)num);
    } else {
      snprintf(buf, sizeof(buf), "%.17g", num);
    }
    return sb_append_str(s, buf);
  }

  case JSON_STRING:
    return stringify_string(s, v->v.string);

  case JSON_ARRAY:
    return stringify_array(s, v);

  case JSON_OBJECT:
    return stringify_object(s, v);
  }

  return false;
}

char *json_stringify(const json_value_t *value, bool pretty) {
  stringifier_t s = {
      .buf = malloc(256), .len = 0, .cap = 256, .pretty = pretty, .indent = 0};

  if (!s.buf)
    return NULL;
  s.buf[0] = '\0';

  if (!stringify_value(&s, value)) {
    free(s.buf);
    return NULL;
  }

  return s.buf;
}

/* ========== Value constructors ========== */

json_value_t *json_null(void) {
  json_value_t *v = calloc(1, sizeof(json_value_t));
  if (v)
    v->type = JSON_NULL;
  return v;
}

json_value_t *json_bool(bool val) {
  json_value_t *v = calloc(1, sizeof(json_value_t));
  if (v) {
    v->type = JSON_BOOL;
    v->v.boolean = val;
  }
  return v;
}

json_value_t *json_number(double val) {
  json_value_t *v = calloc(1, sizeof(json_value_t));
  if (v) {
    v->type = JSON_NUMBER;
    v->v.number = val;
  }
  return v;
}

json_value_t *json_string(const char *val) {
  return json_string_n(val, strlen(val));
}

json_value_t *json_string_n(const char *val, size_t len) {
  json_value_t *v = calloc(1, sizeof(json_value_t));
  if (!v)
    return NULL;

  v->type = JSON_STRING;
  v->v.string = malloc(len + 1);
  if (!v->v.string) {
    free(v);
    return NULL;
  }
  memcpy(v->v.string, val, len);
  v->v.string[len] = '\0';
  return v;
}

json_value_t *json_array_new(void) {
  json_value_t *v = calloc(1, sizeof(json_value_t));
  if (!v)
    return NULL;

  v->type = JSON_ARRAY;
  v->v.array.items = NULL;
  v->v.array.count = 0;
  v->v.array.capacity = 0;
  return v;
}

json_value_t *json_object_new(void) {
  json_value_t *v = calloc(1, sizeof(json_value_t));
  if (!v)
    return NULL;

  v->type = JSON_OBJECT;
  v->v.object.pairs = NULL;
  v->v.object.count = 0;
  v->v.object.capacity = 0;
  return v;
}

/* ========== Array operations ========== */

bool json_array_push(json_value_t *arr, json_value_t *val) {
  if (!arr || arr->type != JSON_ARRAY)
    return false;

  if (arr->v.array.count >= arr->v.array.capacity) {
    size_t new_cap = arr->v.array.capacity == 0 ? INITIAL_CAPACITY
                                                : arr->v.array.capacity * 2;
    json_value_t **new_items =
        realloc(arr->v.array.items, new_cap * sizeof(json_value_t *));
    if (!new_items)
      return false;
    arr->v.array.items = new_items;
    arr->v.array.capacity = new_cap;
  }

  arr->v.array.items[arr->v.array.count++] = val;
  return true;
}

json_value_t *json_array_get(const json_value_t *arr, size_t index) {
  if (!arr || arr->type != JSON_ARRAY || index >= arr->v.array.count)
    return NULL;
  return arr->v.array.items[index];
}

size_t json_array_len(const json_value_t *arr) {
  if (!arr || arr->type != JSON_ARRAY)
    return 0;
  return arr->v.array.count;
}

/* ========== Object operations ========== */

bool json_object_set(json_value_t *obj, const char *key, json_value_t *val) {
  if (!obj || obj->type != JSON_OBJECT || !key)
    return false;

  /* Check for existing key */
  for (size_t i = 0; i < obj->v.object.count; i++) {
    if (strcmp(obj->v.object.pairs[i].key, key) == 0) {
      json_free(obj->v.object.pairs[i].value);
      obj->v.object.pairs[i].value = val;
      return true;
    }
  }

  /* Add new key */
  if (obj->v.object.count >= obj->v.object.capacity) {
    size_t new_cap = obj->v.object.capacity == 0 ? INITIAL_CAPACITY
                                                 : obj->v.object.capacity * 2;
    json_pair_t *new_pairs =
        realloc(obj->v.object.pairs, new_cap * sizeof(json_pair_t));
    if (!new_pairs)
      return false;
    obj->v.object.pairs = new_pairs;
    obj->v.object.capacity = new_cap;
  }

  char *key_copy = strdup(key);
  if (!key_copy)
    return false;

  obj->v.object.pairs[obj->v.object.count].key = key_copy;
  obj->v.object.pairs[obj->v.object.count].value = val;
  obj->v.object.count++;
  return true;
}

json_value_t *json_object_get(const json_value_t *obj, const char *key) {
  if (!obj || obj->type != JSON_OBJECT || !key)
    return NULL;

  for (size_t i = 0; i < obj->v.object.count; i++) {
    if (strcmp(obj->v.object.pairs[i].key, key) == 0) {
      return obj->v.object.pairs[i].value;
    }
  }
  return NULL;
}

bool json_object_has(const json_value_t *obj, const char *key) {
  return json_object_get(obj, key) != NULL;
}

size_t json_object_len(const json_value_t *obj) {
  if (!obj || obj->type != JSON_OBJECT)
    return 0;
  return obj->v.object.count;
}

/* ========== Cleanup ========== */

void json_free(json_value_t *value) {
  if (!value)
    return;

  switch (value->type) {
  case JSON_STRING:
    free(value->v.string);
    break;

  case JSON_ARRAY:
    for (size_t i = 0; i < value->v.array.count; i++) {
      json_free(value->v.array.items[i]);
    }
    free(value->v.array.items);
    break;

  case JSON_OBJECT:
    for (size_t i = 0; i < value->v.object.count; i++) {
      free(value->v.object.pairs[i].key);
      json_free(value->v.object.pairs[i].value);
    }
    free(value->v.object.pairs);
    break;

  default:
    break;
  }

  free(value);
}

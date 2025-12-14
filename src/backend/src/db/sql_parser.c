/**
 * SQL Parser - Recursive Descent Parser for SQL Subset
 */
#include "sql_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Token types */
typedef enum {
  TOK_EOF,
  TOK_KEYWORD,
  TOK_IDENTIFIER,
  TOK_STRING,
  TOK_NUMBER,
  TOK_REAL,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_COMMA,
  TOK_STAR,
  TOK_EQ,
  TOK_NE,
  TOK_LT,
  TOK_LE,
  TOK_GT,
  TOK_GE,
  TOK_SEMICOLON,
  TOK_DOT
} token_type_t;

/* Token */
typedef struct {
  token_type_t type;
  char *value; /* Dynamic string */
  int64_t int_val;
  double real_val;
} token_t;

/* Lexer state */
typedef struct {
  const char *sql;
  size_t pos;
  size_t len;
  token_t current;
  char error[256];
} lexer_t;

/* SQL keywords */
static const char *keywords[] = {
    "SELECT",  "FROM",   "WHERE",   "INSERT", "INTO",   "VALUES", "UPDATE",
    "SET",     "DELETE", "CREATE",  "TABLE",  "DROP",   "AND",    "OR",
    "NOT",     "NULL",   "IS",      "LIKE",   "ORDER",  "BY",     "ASC",
    "DESC",    "LIMIT",  "INTEGER", "TEXT",   "REAL",   "BLOB",   "TIMESTAMP",
    "PRIMARY", "KEY",    "UNIQUE",  "IF",     "EXISTS", NULL};

static bool is_keyword(const char *word) {
  for (int i = 0; keywords[i]; i++) {
    if (strcasecmp(word, keywords[i]) == 0)
      return true;
  }
  return false;
}

static void skip_whitespace(lexer_t *lex) {
  while (lex->pos < lex->len && isspace((unsigned char)lex->sql[lex->pos])) {
    lex->pos++;
  }
}

static void lexer_cleanup(lexer_t *lex) {
  free(lex->current.value);
  lex->current.value = NULL;
}

static char *strndup_safe(const char *s, size_t n) {
  char *d = malloc(n + 1);
  if (d) {
    memcpy(d, s, n);
    d[n] = '\0';
  }
  return d;
}

static bool next_token(lexer_t *lex) {
  /* Free previous token value */
  free(lex->current.value);
  lex->current.value = NULL;

  skip_whitespace(lex);

  if (lex->pos >= lex->len) {
    lex->current.type = TOK_EOF;
    return true;
  }

  char c = lex->sql[lex->pos];

  /* Single character tokens */
  switch (c) {
  case '(':
    lex->current.type = TOK_LPAREN;
    lex->current.value = strndup_safe("(", 1);
    lex->pos++;
    return true;
  case ')':
    lex->current.type = TOK_RPAREN;
    lex->current.value = strndup_safe(")", 1);
    lex->pos++;
    return true;
  case ',':
    lex->current.type = TOK_COMMA;
    lex->current.value = strndup_safe(",", 1);
    lex->pos++;
    return true;
  case '*':
    lex->current.type = TOK_STAR;
    lex->current.value = strndup_safe("*", 1);
    lex->pos++;
    return true;
  case ';':
    lex->current.type = TOK_SEMICOLON;
    lex->current.value = strndup_safe(";", 1);
    lex->pos++;
    return true;
  case '.':
    lex->current.type = TOK_DOT;
    lex->current.value = strndup_safe(".", 1);
    lex->pos++;
    return true;
  }

  /* Operators */
  if (c == '=') {
    lex->current.type = TOK_EQ;
    lex->current.value = strndup_safe("=", 1);
    lex->pos++;
    return true;
  }
  if (c == '!' && lex->pos + 1 < lex->len && lex->sql[lex->pos + 1] == '=') {
    lex->current.type = TOK_NE;
    lex->current.value = strndup_safe("!=", 2);
    lex->pos += 2;
    return true;
  }
  if (c == '<') {
    if (lex->pos + 1 < lex->len && lex->sql[lex->pos + 1] == '>') {
      lex->current.type = TOK_NE;
      lex->current.value = strndup_safe("<>", 2);
      lex->pos += 2;
    } else if (lex->pos + 1 < lex->len && lex->sql[lex->pos + 1] == '=') {
      lex->current.type = TOK_LE;
      lex->current.value = strndup_safe("<=", 2);
      lex->pos += 2;
    } else {
      lex->current.type = TOK_LT;
      lex->current.value = strndup_safe("<", 1);
      lex->pos++;
    }
    return true;
  }
  if (c == '>') {
    if (lex->pos + 1 < lex->len && lex->sql[lex->pos + 1] == '=') {
      lex->current.type = TOK_GE;
      lex->current.value = strndup_safe(">=", 2);
      lex->pos += 2;
    } else {
      lex->current.type = TOK_GT;
      lex->current.value = strndup_safe(">", 1);
      lex->pos++;
    }
    return true;
  }

  /* String literal */
  if (c == '\'' || c == '"') {
    char quote = c;
    lex->pos++;
    size_t start = lex->pos;
    while (lex->pos < lex->len && lex->sql[lex->pos] != quote) {
      if (lex->sql[lex->pos] == '\\' && lex->pos + 1 < lex->len) {
        lex->pos++; /* Skip escape */
      }
      lex->pos++;
    }
    size_t len = lex->pos - start;
    lex->current.value = strndup_safe(&lex->sql[start], len);
    lex->current.type = TOK_STRING;
    lex->pos++; /* Skip closing quote */
    return true;
  }

  /* Number */
  if (isdigit((unsigned char)c) ||
      (c == '-' && lex->pos + 1 < lex->len &&
       isdigit((unsigned char)lex->sql[lex->pos + 1]))) {
    size_t start = lex->pos;
    if (c == '-')
      lex->pos++;
    while (lex->pos < lex->len && isdigit((unsigned char)lex->sql[lex->pos])) {
      lex->pos++;
    }
    bool is_real = false;
    if (lex->pos < lex->len && lex->sql[lex->pos] == '.') {
      is_real = true;
      lex->pos++;
      while (lex->pos < lex->len &&
             isdigit((unsigned char)lex->sql[lex->pos])) {
        lex->pos++;
      }
    }
    size_t len = lex->pos - start;
    lex->current.value = strndup_safe(&lex->sql[start], len);

    if (is_real) {
      lex->current.type = TOK_REAL;
      lex->current.real_val = strtod(lex->current.value, NULL);
    } else {
      lex->current.type = TOK_NUMBER;
      lex->current.int_val = strtoll(lex->current.value, NULL, 10);
    }
    return true;
  }

  /* Identifier or keyword */
  if (isalpha((unsigned char)c) || c == '_') {
    size_t start = lex->pos;
    while (lex->pos < lex->len && (isalnum((unsigned char)lex->sql[lex->pos]) ||
                                   lex->sql[lex->pos] == '_')) {
      lex->pos++;
    }
    size_t len = lex->pos - start;
    lex->current.value = strndup_safe(&lex->sql[start], len);

    lex->current.type =
        is_keyword(lex->current.value) ? TOK_KEYWORD : TOK_IDENTIFIER;
    return true;
  }

  snprintf(lex->error, sizeof(lex->error), "Unexpected character: '%c'", c);
  return false;
}

static bool match_keyword(lexer_t *lex, const char *kw) {
  if (lex->current.type == TOK_KEYWORD &&
      strcasecmp(lex->current.value, kw) == 0) {
    next_token(lex);
    return true;
  }
  return false;
}

static bool expect_keyword(lexer_t *lex, const char *kw) {
  if (!match_keyword(lex, kw)) {
    snprintf(lex->error, sizeof(lex->error), "Expected keyword '%s'", kw);
    return false;
  }
  return true;
}

static bool match_token(lexer_t *lex, token_type_t type) {
  if (lex->current.type == type) {
    next_token(lex);
    return true;
  }
  return false;
}

static char *copy_string(const char *s) {
  size_t len = strlen(s);
  char *copy = malloc(len + 1);
  if (copy)
    memcpy(copy, s, len + 1);
  return copy;
}

/* Forward declarations */
static expr_t *parse_expression(lexer_t *lex);
static expr_t *parse_or_expr(lexer_t *lex);
static expr_t *parse_and_expr(lexer_t *lex);
static expr_t *parse_compare_expr(lexer_t *lex);
static expr_t *parse_primary_expr(lexer_t *lex);

/*
 * Parse primary expression (column, value, or parenthesized expression)
 */
static expr_t *parse_primary_expr(lexer_t *lex) {
  expr_t *expr = calloc(1, sizeof(expr_t));
  if (!expr)
    return NULL;

  if (match_token(lex, TOK_LPAREN)) {
    expr_t *inner = parse_expression(lex);
    if (!inner || !match_token(lex, TOK_RPAREN)) {
      sql_expr_free(inner);
      free(expr);
      return NULL;
    }
    free(expr);
    return inner;
  }

  if (lex->current.type == TOK_IDENTIFIER) {
    expr->type = EXPR_COLUMN;
    expr->v.column = copy_string(lex->current.value);
    next_token(lex);
    return expr;
  }

  if (lex->current.type == TOK_STRING) {
    expr->type = EXPR_VALUE;
    expr->v.value.type = KDB_TYPE_TEXT;
    expr->v.value.v.text.data = copy_string(lex->current.value);
    expr->v.value.v.text.len = strlen(lex->current.value);
    next_token(lex);
    return expr;
  }

  if (lex->current.type == TOK_NUMBER) {
    expr->type = EXPR_VALUE;
    expr->v.value.type = KDB_TYPE_INTEGER;
    expr->v.value.v.i = lex->current.int_val;
    next_token(lex);
    return expr;
  }

  if (lex->current.type == TOK_REAL) {
    expr->type = EXPR_VALUE;
    expr->v.value.type = KDB_TYPE_REAL;
    expr->v.value.v.r = lex->current.real_val;
    next_token(lex);
    return expr;
  }

  if (match_keyword(lex, "NULL")) {
    expr->type = EXPR_VALUE;
    expr->v.value.type = KDB_TYPE_NULL;
    return expr;
  }

  snprintf(lex->error, sizeof(lex->error), "Expected expression");
  free(expr);
  return NULL;
}

/*
 * Parse comparison expression
 */
static expr_t *parse_compare_expr(lexer_t *lex) {
  expr_t *left = parse_primary_expr(lex);
  if (!left)
    return NULL;

  /* Check for IS NULL / IS NOT NULL */
  if (match_keyword(lex, "IS")) {
    expr_t *cmp = calloc(1, sizeof(expr_t));
    cmp->type = EXPR_COMPARE;
    cmp->v.compare.left = left;

    if (match_keyword(lex, "NOT")) {
      cmp->v.compare.op = OP_IS_NOT_NULL;
    } else {
      cmp->v.compare.op = OP_IS_NULL;
    }

    if (!expect_keyword(lex, "NULL")) {
      sql_expr_free(cmp);
      return NULL;
    }

    return cmp;
  }

  compare_op_t op;
  bool has_op = true;

  if (lex->current.type == TOK_EQ)
    op = OP_EQ;
  else if (lex->current.type == TOK_NE)
    op = OP_NE;
  else if (lex->current.type == TOK_LT)
    op = OP_LT;
  else if (lex->current.type == TOK_LE)
    op = OP_LE;
  else if (lex->current.type == TOK_GT)
    op = OP_GT;
  else if (lex->current.type == TOK_GE)
    op = OP_GE;
  else if (match_keyword(lex, "LIKE")) {
    op = OP_LIKE;
    goto parse_right;
  } else {
    has_op = false;
  }

  if (!has_op) {
    return left;
  }

  next_token(lex);

parse_right:;
  expr_t *right = parse_primary_expr(lex);
  if (!right) {
    sql_expr_free(left);
    return NULL;
  }

  expr_t *cmp = calloc(1, sizeof(expr_t));
  cmp->type = EXPR_COMPARE;
  cmp->v.compare.left = left;
  cmp->v.compare.op = op;
  cmp->v.compare.right = right;

  return cmp;
}

/*
 * Parse AND expression
 */
static expr_t *parse_and_expr(lexer_t *lex) {
  expr_t *left = parse_compare_expr(lex);
  if (!left)
    return NULL;

  while (match_keyword(lex, "AND")) {
    expr_t *right = parse_compare_expr(lex);
    if (!right) {
      sql_expr_free(left);
      return NULL;
    }

    expr_t *logic = calloc(1, sizeof(expr_t));
    logic->type = EXPR_LOGIC;
    logic->v.logic.left = left;
    logic->v.logic.op = LOGIC_AND;
    logic->v.logic.right = right;
    left = logic;
  }

  return left;
}

/*
 * Parse OR expression
 */
static expr_t *parse_or_expr(lexer_t *lex) {
  expr_t *left = parse_and_expr(lex);
  if (!left)
    return NULL;

  while (match_keyword(lex, "OR")) {
    expr_t *right = parse_and_expr(lex);
    if (!right) {
      sql_expr_free(left);
      return NULL;
    }

    expr_t *logic = calloc(1, sizeof(expr_t));
    logic->type = EXPR_LOGIC;
    logic->v.logic.left = left;
    logic->v.logic.op = LOGIC_OR;
    logic->v.logic.right = right;
    left = logic;
  }

  return left;
}

/*
 * Parse full expression
 */
static expr_t *parse_expression(lexer_t *lex) { return parse_or_expr(lex); }

/*
 * Parse column type
 */
static bool parse_column_type(lexer_t *lex, kdb_type_t *type) {
  if (match_keyword(lex, "INTEGER")) {
    *type = KDB_TYPE_INTEGER;
    return true;
  }
  if (match_keyword(lex, "TEXT")) {
    *type = KDB_TYPE_TEXT;
    return true;
  }
  if (match_keyword(lex, "REAL")) {
    *type = KDB_TYPE_REAL;
    return true;
  }
  if (match_keyword(lex, "BLOB")) {
    *type = KDB_TYPE_BLOB;
    return true;
  }
  if (match_keyword(lex, "TIMESTAMP")) {
    *type = KDB_TYPE_TIMESTAMP;
    return true;
  }
  snprintf(lex->error, sizeof(lex->error), "Expected column type");
  return false;
}

/*
 * Parse SELECT statement
 */
static kdb_error_t parse_select(lexer_t *lex, kdb_stmt_t *stmt) {
  stmt->type = STMT_SELECT;
  select_stmt_t *sel = &stmt->stmt.select;
  memset(sel, 0, sizeof(*sel));
  sel->limit = -1;

  /* Parse column list */
  if (match_token(lex, TOK_STAR)) {
    sel->columns = NULL;
    sel->num_columns = 0;
  } else {
    int cap = 8;
    sel->columns = malloc(cap * sizeof(char *));

    do {
      if (lex->current.type != TOK_IDENTIFIER) {
        snprintf(lex->error, sizeof(lex->error), "Expected column name");
        return KDB_ERR_SYNTAX;
      }

      if (sel->num_columns >= cap) {
        cap *= 2;
        sel->columns = realloc(sel->columns, cap * sizeof(char *));
      }
      sel->columns[sel->num_columns++] = copy_string(lex->current.value);
      next_token(lex);
    } while (match_token(lex, TOK_COMMA));
  }

  /* FROM clause */
  if (!expect_keyword(lex, "FROM"))
    return KDB_ERR_SYNTAX;

  if (lex->current.type != TOK_IDENTIFIER) {
    snprintf(lex->error, sizeof(lex->error), "Expected table name");
    return KDB_ERR_SYNTAX;
  }
  sel->table = copy_string(lex->current.value);
  next_token(lex);

  /* Optional WHERE */
  if (match_keyword(lex, "WHERE")) {
    sel->where = parse_expression(lex);
    if (!sel->where)
      return KDB_ERR_SYNTAX;
  }

  /* Optional ORDER BY */
  if (match_keyword(lex, "ORDER")) {
    if (!expect_keyword(lex, "BY"))
      return KDB_ERR_SYNTAX;
    if (lex->current.type != TOK_IDENTIFIER) {
      snprintf(lex->error, sizeof(lex->error),
               "Expected column name after ORDER BY");
      return KDB_ERR_SYNTAX;
    }
    sel->order_by = copy_string(lex->current.value);
    next_token(lex);

    if (match_keyword(lex, "DESC")) {
      sel->order_desc = true;
    } else {
      match_keyword(lex, "ASC");
      sel->order_desc = false;
    }
  }

  /* Optional LIMIT */
  if (match_keyword(lex, "LIMIT")) {
    if (lex->current.type != TOK_NUMBER) {
      snprintf(lex->error, sizeof(lex->error), "Expected number after LIMIT");
      return KDB_ERR_SYNTAX;
    }
    sel->limit = (int)lex->current.int_val;
    next_token(lex);
  }

  return KDB_OK;
}

/*
 * Parse INSERT statement
 */
static kdb_error_t parse_insert(lexer_t *lex, kdb_stmt_t *stmt) {
  stmt->type = STMT_INSERT;
  insert_stmt_t *ins = &stmt->stmt.insert;
  memset(ins, 0, sizeof(*ins));

  if (!expect_keyword(lex, "INTO"))
    return KDB_ERR_SYNTAX;

  if (lex->current.type != TOK_IDENTIFIER) {
    snprintf(lex->error, sizeof(lex->error), "Expected table name");
    return KDB_ERR_SYNTAX;
  }
  ins->table = copy_string(lex->current.value);
  next_token(lex);

  /* Optional column list */
  if (match_token(lex, TOK_LPAREN)) {
    int cap = 8;
    ins->columns = malloc(cap * sizeof(char *));

    do {
      if (lex->current.type != TOK_IDENTIFIER) {
        snprintf(lex->error, sizeof(lex->error), "Expected column name");
        return KDB_ERR_SYNTAX;
      }
      if (ins->num_columns >= cap) {
        cap *= 2;
        ins->columns = realloc(ins->columns, cap * sizeof(char *));
      }
      ins->columns[ins->num_columns++] = copy_string(lex->current.value);
      next_token(lex);
    } while (match_token(lex, TOK_COMMA));

    if (!match_token(lex, TOK_RPAREN)) {
      snprintf(lex->error, sizeof(lex->error), "Expected ')'");
      return KDB_ERR_SYNTAX;
    }
  }

  if (!expect_keyword(lex, "VALUES"))
    return KDB_ERR_SYNTAX;
  if (!match_token(lex, TOK_LPAREN)) {
    snprintf(lex->error, sizeof(lex->error), "Expected '(' after VALUES");
    return KDB_ERR_SYNTAX;
  }

  /* Parse values */
  int cap = 8;
  ins->values = malloc(cap * sizeof(kdb_value_t));

  do {
    if (ins->num_values >= cap) {
      cap *= 2;
      ins->values = realloc(ins->values, cap * sizeof(kdb_value_t));
    }

    kdb_value_t *val = &ins->values[ins->num_values++];
    memset(val, 0, sizeof(*val));

    if (match_keyword(lex, "NULL")) {
      val->type = KDB_TYPE_NULL;
    } else if (lex->current.type == TOK_STRING) {
      val->type = KDB_TYPE_TEXT;
      val->v.text.data = copy_string(lex->current.value);
      val->v.text.len = strlen(lex->current.value);
      next_token(lex);
    } else if (lex->current.type == TOK_NUMBER) {
      val->type = KDB_TYPE_INTEGER;
      val->v.i = lex->current.int_val;
      next_token(lex);
    } else if (lex->current.type == TOK_REAL) {
      val->type = KDB_TYPE_REAL;
      val->v.r = lex->current.real_val;
      next_token(lex);
    } else {
      snprintf(lex->error, sizeof(lex->error), "Expected value");
      return KDB_ERR_SYNTAX;
    }
  } while (match_token(lex, TOK_COMMA));

  if (!match_token(lex, TOK_RPAREN)) {
    snprintf(lex->error, sizeof(lex->error), "Expected ')'");
    return KDB_ERR_SYNTAX;
  }

  return KDB_OK;
}

/*
 * Parse UPDATE statement
 */
static kdb_error_t parse_update(lexer_t *lex, kdb_stmt_t *stmt) {
  stmt->type = STMT_UPDATE;
  update_stmt_t *upd = &stmt->stmt.update;
  memset(upd, 0, sizeof(*upd));

  if (lex->current.type != TOK_IDENTIFIER) {
    snprintf(lex->error, sizeof(lex->error), "Expected table name");
    return KDB_ERR_SYNTAX;
  }
  upd->table = copy_string(lex->current.value);
  next_token(lex);

  if (!expect_keyword(lex, "SET"))
    return KDB_ERR_SYNTAX;

  /* Parse assignments */
  int cap = 8;
  upd->assignments = malloc(cap * sizeof(assignment_t));

  do {
    if (upd->num_assignments >= cap) {
      cap *= 2;
      upd->assignments = realloc(upd->assignments, cap * sizeof(assignment_t));
    }

    assignment_t *asgn = &upd->assignments[upd->num_assignments++];
    memset(asgn, 0, sizeof(*asgn));

    if (lex->current.type != TOK_IDENTIFIER) {
      snprintf(lex->error, sizeof(lex->error), "Expected column name");
      return KDB_ERR_SYNTAX;
    }
    asgn->column = copy_string(lex->current.value);
    next_token(lex);

    if (!match_token(lex, TOK_EQ)) {
      snprintf(lex->error, sizeof(lex->error), "Expected '='");
      return KDB_ERR_SYNTAX;
    }

    if (match_keyword(lex, "NULL")) {
      asgn->value.type = KDB_TYPE_NULL;
    } else if (lex->current.type == TOK_STRING) {
      asgn->value.type = KDB_TYPE_TEXT;
      asgn->value.v.text.data = copy_string(lex->current.value);
      asgn->value.v.text.len = strlen(lex->current.value);
      next_token(lex);
    } else if (lex->current.type == TOK_NUMBER) {
      asgn->value.type = KDB_TYPE_INTEGER;
      asgn->value.v.i = lex->current.int_val;
      next_token(lex);
    } else if (lex->current.type == TOK_REAL) {
      asgn->value.type = KDB_TYPE_REAL;
      asgn->value.v.r = lex->current.real_val;
      next_token(lex);
    } else {
      snprintf(lex->error, sizeof(lex->error), "Expected value");
      return KDB_ERR_SYNTAX;
    }
  } while (match_token(lex, TOK_COMMA));

  /* Optional WHERE */
  if (match_keyword(lex, "WHERE")) {
    upd->where = parse_expression(lex);
    if (!upd->where)
      return KDB_ERR_SYNTAX;
  }

  return KDB_OK;
}

/*
 * Parse DELETE statement
 */
static kdb_error_t parse_delete(lexer_t *lex, kdb_stmt_t *stmt) {
  stmt->type = STMT_DELETE;
  delete_stmt_t *del = &stmt->stmt.del;
  memset(del, 0, sizeof(*del));

  if (!expect_keyword(lex, "FROM"))
    return KDB_ERR_SYNTAX;

  if (lex->current.type != TOK_IDENTIFIER) {
    snprintf(lex->error, sizeof(lex->error), "Expected table name");
    return KDB_ERR_SYNTAX;
  }
  del->table = copy_string(lex->current.value);
  next_token(lex);

  /* Optional WHERE */
  if (match_keyword(lex, "WHERE")) {
    del->where = parse_expression(lex);
    if (!del->where)
      return KDB_ERR_SYNTAX;
  }

  return KDB_OK;
}

/*
 * Parse CREATE TABLE statement
 */
static kdb_error_t parse_create_table(lexer_t *lex, kdb_stmt_t *stmt) {
  stmt->type = STMT_CREATE_TABLE;
  create_table_stmt_t *ct = &stmt->stmt.create_table;
  memset(ct, 0, sizeof(*ct));

  if (!expect_keyword(lex, "TABLE"))
    return KDB_ERR_SYNTAX;

  if (lex->current.type != TOK_IDENTIFIER) {
    snprintf(lex->error, sizeof(lex->error), "Expected table name");
    return KDB_ERR_SYNTAX;
  }
  ct->table = copy_string(lex->current.value);
  next_token(lex);

  if (!match_token(lex, TOK_LPAREN)) {
    snprintf(lex->error, sizeof(lex->error), "Expected '('");
    return KDB_ERR_SYNTAX;
  }

  /* Parse column definitions */
  do {
    if (ct->num_columns >= KDB_MAX_COLUMNS) {
      snprintf(lex->error, sizeof(lex->error), "Too many columns");
      return KDB_ERR_SYNTAX;
    }

    kdb_column_def_t *col = &ct->columns[ct->num_columns++];
    memset(col, 0, sizeof(*col));

    if (lex->current.type != TOK_IDENTIFIER) {
      snprintf(lex->error, sizeof(lex->error), "Expected column name");
      return KDB_ERR_SYNTAX;
    }
    strncpy(col->name, lex->current.value, sizeof(col->name) - 1);
    next_token(lex);

    if (!parse_column_type(lex, &col->type))
      return KDB_ERR_SYNTAX;

    /* Optional constraints */
    while (true) {
      if (match_keyword(lex, "PRIMARY")) {
        if (!expect_keyword(lex, "KEY"))
          return KDB_ERR_SYNTAX;
        col->primary_key = true;
      } else if (match_keyword(lex, "UNIQUE")) {
        col->unique = true;
      } else if (match_keyword(lex, "NOT")) {
        if (!expect_keyword(lex, "NULL"))
          return KDB_ERR_SYNTAX;
        col->not_null = true;
      } else {
        break;
      }
    }
  } while (match_token(lex, TOK_COMMA));

  if (!match_token(lex, TOK_RPAREN)) {
    snprintf(lex->error, sizeof(lex->error), "Expected ')'");
    return KDB_ERR_SYNTAX;
  }

  return KDB_OK;
}

/*
 * Parse DROP TABLE statement
 */
static kdb_error_t parse_drop_table(lexer_t *lex, kdb_stmt_t *stmt) {
  stmt->type = STMT_DROP_TABLE;
  drop_table_stmt_t *dt = &stmt->stmt.drop_table;
  memset(dt, 0, sizeof(*dt));

  if (!expect_keyword(lex, "TABLE"))
    return KDB_ERR_SYNTAX;

  /* Optional IF EXISTS */
  if (match_keyword(lex, "IF")) {
    if (!expect_keyword(lex, "EXISTS"))
      return KDB_ERR_SYNTAX;
    dt->if_exists = true;
  }

  if (lex->current.type != TOK_IDENTIFIER) {
    snprintf(lex->error, sizeof(lex->error), "Expected table name");
    return KDB_ERR_SYNTAX;
  }
  dt->table = copy_string(lex->current.value);
  next_token(lex);

  return KDB_OK;
}

/*
 * Main parse entry point
 */
kdb_error_t sql_parse(const char *sql, kdb_stmt_t *stmt, char *err_msg,
                      size_t err_len) {
  if (!sql || !stmt) {
    if (err_msg)
      snprintf(err_msg, err_len, "Invalid arguments");
    return KDB_ERR_SYNTAX;
  }

  lexer_t lex = {0};
  lex.sql = sql;
  lex.len = strlen(sql);
  lex.pos = 0;

  if (!next_token(&lex)) {
    if (err_msg)
      snprintf(err_msg, err_len, "%s", lex.error);
    lexer_cleanup(&lex);
    return KDB_ERR_SYNTAX;
  }

  kdb_error_t err = KDB_OK;

  if (match_keyword(&lex, "SELECT")) {
    err = parse_select(&lex, stmt);
  } else if (match_keyword(&lex, "INSERT")) {
    err = parse_insert(&lex, stmt);
  } else if (match_keyword(&lex, "UPDATE")) {
    err = parse_update(&lex, stmt);
  } else if (match_keyword(&lex, "DELETE")) {
    err = parse_delete(&lex, stmt);
  } else if (match_keyword(&lex, "CREATE")) {
    err = parse_create_table(&lex, stmt);
  } else if (match_keyword(&lex, "DROP")) {
    err = parse_drop_table(&lex, stmt);
  } else {
    snprintf(lex.error, sizeof(lex.error), "Unknown statement type");
    err = KDB_ERR_SYNTAX;
  }

  if (err != KDB_OK && err_msg) {
    snprintf(err_msg, err_len, "%s", lex.error);
  }

  lexer_cleanup(&lex);
  return err;
}

/*
 * Free expression tree
 */
void sql_expr_free(expr_t *expr) {
  if (!expr)
    return;

  switch (expr->type) {
  case EXPR_COLUMN:
    free(expr->v.column);
    break;
  case EXPR_VALUE:
    kdb_value_free(&expr->v.value);
    break;
  case EXPR_COMPARE:
    sql_expr_free(expr->v.compare.left);
    sql_expr_free(expr->v.compare.right);
    break;
  case EXPR_LOGIC:
    sql_expr_free(expr->v.logic.left);
    sql_expr_free(expr->v.logic.right);
    break;
  }

  free(expr);
}

/*
 * Free statement resources
 */
void sql_stmt_free(kdb_stmt_t *stmt) {
  if (!stmt)
    return;

  switch (stmt->type) {
  case STMT_SELECT: {
    select_stmt_t *sel = &stmt->stmt.select;
    for (int i = 0; i < sel->num_columns; i++) {
      free(sel->columns[i]);
    }
    free(sel->columns);
    free(sel->table);
    free(sel->order_by);
    sql_expr_free(sel->where);
    break;
  }
  case STMT_INSERT: {
    insert_stmt_t *ins = &stmt->stmt.insert;
    free(ins->table);
    for (int i = 0; i < ins->num_columns; i++) {
      free(ins->columns[i]);
    }
    free(ins->columns);
    for (int i = 0; i < ins->num_values; i++) {
      kdb_value_free(&ins->values[i]);
    }
    free(ins->values);
    break;
  }
  case STMT_UPDATE: {
    update_stmt_t *upd = &stmt->stmt.update;
    free(upd->table);
    for (int i = 0; i < upd->num_assignments; i++) {
      free(upd->assignments[i].column);
      kdb_value_free(&upd->assignments[i].value);
    }
    free(upd->assignments);
    sql_expr_free(upd->where);
    break;
  }
  case STMT_DELETE: {
    delete_stmt_t *del = &stmt->stmt.del;
    free(del->table);
    sql_expr_free(del->where);
    break;
  }
  case STMT_CREATE_TABLE: {
    create_table_stmt_t *ct = &stmt->stmt.create_table;
    free(ct->table);
    break;
  }
  case STMT_DROP_TABLE: {
    drop_table_stmt_t *dt = &stmt->stmt.drop_table;
    free(dt->table);
    break;
  }
  }
}

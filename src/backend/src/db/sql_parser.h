/**
 * SQL Parser - Recursive Descent Parser for SQL Subset
 */
#ifndef KDB_SQL_PARSER_H
#define KDB_SQL_PARSER_H

#include "db.h"
#include <stdbool.h>

/* Statement types */
typedef enum {
  STMT_SELECT,
  STMT_INSERT,
  STMT_UPDATE,
  STMT_DELETE,
  STMT_CREATE_TABLE,
  STMT_DROP_TABLE
} stmt_type_t;

/* Comparison operators */
typedef enum {
  OP_EQ,      /* = */
  OP_NE,      /* != or <> */
  OP_LT,      /* < */
  OP_LE,      /* <= */
  OP_GT,      /* > */
  OP_GE,      /* >= */
  OP_LIKE,    /* LIKE */
  OP_IS_NULL, /* IS NULL */
  OP_IS_NOT_NULL
} compare_op_t;

/* Logical operators */
typedef enum { LOGIC_AND, LOGIC_OR } logic_op_t;

/* Expression node (for WHERE clause) */
typedef struct expr_s {
  enum { EXPR_COLUMN, EXPR_VALUE, EXPR_COMPARE, EXPR_LOGIC } type;
  union {
    char *column;      /* EXPR_COLUMN */
    kdb_value_t value; /* EXPR_VALUE */
    struct {           /* EXPR_COMPARE */
      struct expr_s *left;
      compare_op_t op;
      struct expr_s *right;
    } compare;
    struct { /* EXPR_LOGIC */
      struct expr_s *left;
      logic_op_t op;
      struct expr_s *right;
    } logic;
  } v;
} expr_t;

/* Column assignment (for UPDATE SET) */
typedef struct {
  char *column;
  kdb_value_t value;
} assignment_t;

/* SELECT statement */
typedef struct {
  char **columns; /* NULL = SELECT * */
  int num_columns;
  char *table;
  expr_t *where;  /* NULL = no WHERE */
  char *order_by; /* NULL = no ORDER BY */
  bool order_desc;
  int limit; /* -1 = no limit */
} select_stmt_t;

/* INSERT statement */
typedef struct {
  char *table;
  char **columns; /* NULL = all columns */
  int num_columns;
  kdb_value_t *values;
  int num_values;
} insert_stmt_t;

/* UPDATE statement */
typedef struct {
  char *table;
  assignment_t *assignments;
  int num_assignments;
  expr_t *where; /* NULL = update all */
} update_stmt_t;

/* DELETE statement */
typedef struct {
  char *table;
  expr_t *where; /* NULL = delete all */
} delete_stmt_t;

/* CREATE TABLE statement */
typedef struct {
  char *table;
  kdb_column_def_t columns[KDB_MAX_COLUMNS];
  int num_columns;
} create_table_stmt_t;

/* DROP TABLE statement */
typedef struct {
  char *table;
  bool if_exists;
} drop_table_stmt_t;

/* Parsed statement */
typedef struct {
  stmt_type_t type;
  union {
    select_stmt_t select;
    insert_stmt_t insert;
    update_stmt_t update;
    delete_stmt_t del;
    create_table_stmt_t create_table;
    drop_table_stmt_t drop_table;
  } stmt;
} kdb_stmt_t;

/**
 * Parse SQL string into statement structure
 * @param sql     SQL string to parse
 * @param stmt    Output statement (caller owns memory)
 * @param err_msg Output error message on failure
 * @return KDB_OK on success, KDB_ERR_SYNTAX on parse error
 */
kdb_error_t sql_parse(const char *sql, kdb_stmt_t *stmt, char *err_msg,
                      size_t err_len);

/**
 * Free parsed statement resources
 */
void sql_stmt_free(kdb_stmt_t *stmt);

/**
 * Free expression tree
 */
void sql_expr_free(expr_t *expr);

#endif /* KDB_SQL_PARSER_H */

/**
 * JSON Parser Test Suite
 */
#include "../src/json/json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  do {                                                                         \
    printf("  Testing %s... ", name);                                          \
    tests_run++;                                                               \
  } while (0)

#define PASS()                                                                 \
  do {                                                                         \
    printf("\033[32mPASS\033[0m\n");                                           \
    tests_passed++;                                                            \
  } while (0)

#define FAIL(msg)                                                              \
  do {                                                                         \
    printf("\033[31mFAIL\033[0m: %s\n", msg);                                  \
  } while (0)

#define ASSERT(cond, msg)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      FAIL(msg);                                                               \
      return;                                                                  \
    }                                                                          \
  } while (0)

/* ========== Primitive parsing tests ========== */

static void test_parse_null(void) {
  TEST("parse null");

  json_value_t *v = json_parse("null", NULL);
  ASSERT(v != NULL, "Parse failed");
  ASSERT(json_is_null(v), "Not null type");

  json_free(v);
  PASS();
}

static void test_parse_bool(void) {
  TEST("parse bool");

  json_value_t *v = json_parse("true", NULL);
  ASSERT(v != NULL, "Parse true failed");
  ASSERT(json_is_bool(v), "Not bool type");
  ASSERT(json_get_bool(v) == true, "Not true");
  json_free(v);

  v = json_parse("false", NULL);
  ASSERT(v != NULL, "Parse false failed");
  ASSERT(json_get_bool(v) == false, "Not false");
  json_free(v);

  PASS();
}

static void test_parse_number(void) {
  TEST("parse number");

  json_value_t *v = json_parse("42", NULL);
  ASSERT(v != NULL, "Parse int failed");
  ASSERT(json_is_number(v), "Not number type");
  ASSERT(json_get_number(v) == 42.0, "Wrong value");
  json_free(v);

  v = json_parse("-123.456", NULL);
  ASSERT(fabs(json_get_number(v) - (-123.456)) < 0.0001, "Wrong float value");
  json_free(v);

  v = json_parse("1.5e10", NULL);
  ASSERT(json_get_number(v) == 1.5e10, "Wrong exp value");
  json_free(v);

  PASS();
}

static void test_parse_string(void) {
  TEST("parse string");

  json_value_t *v = json_parse("\"hello\"", NULL);
  ASSERT(v != NULL, "Parse failed");
  ASSERT(json_is_string(v), "Not string type");
  ASSERT(strcmp(json_get_string(v), "hello") == 0, "Wrong value");
  json_free(v);

  /* Test escapes */
  v = json_parse("\"hello\\nworld\"", NULL);
  ASSERT(strcmp(json_get_string(v), "hello\nworld") == 0, "Escape failed");
  json_free(v);

  /* Test unicode */
  v = json_parse("\"\\u0048\\u0065\\u006c\\u006c\\u006f\"", NULL);
  ASSERT(strcmp(json_get_string(v), "Hello") == 0, "Unicode failed");
  json_free(v);

  PASS();
}

/* ========== Composite type tests ========== */

static void test_parse_array(void) {
  TEST("parse array");

  json_value_t *v = json_parse("[1, 2, 3]", NULL);
  ASSERT(v != NULL, "Parse failed");
  ASSERT(json_is_array(v), "Not array type");
  ASSERT(json_array_len(v) == 3, "Wrong length");
  ASSERT(json_get_number(json_array_get(v, 0)) == 1, "Wrong item 0");
  ASSERT(json_get_number(json_array_get(v, 1)) == 2, "Wrong item 1");
  ASSERT(json_get_number(json_array_get(v, 2)) == 3, "Wrong item 2");
  json_free(v);

  /* Empty array */
  v = json_parse("[]", NULL);
  ASSERT(json_array_len(v) == 0, "Empty array not empty");
  json_free(v);

  PASS();
}

static void test_parse_object(void) {
  TEST("parse object");

  json_value_t *v = json_parse("{\"name\": \"John\", \"age\": 30}", NULL);
  ASSERT(v != NULL, "Parse failed");
  ASSERT(json_is_object(v), "Not object type");
  ASSERT(json_object_len(v) == 2, "Wrong key count");

  json_value_t *name = json_object_get(v, "name");
  ASSERT(strcmp(json_get_string(name), "John") == 0, "Wrong name");

  json_value_t *age = json_object_get(v, "age");
  ASSERT(json_get_number(age) == 30, "Wrong age");

  json_free(v);

  /* Empty object */
  v = json_parse("{}", NULL);
  ASSERT(json_object_len(v) == 0, "Empty object not empty");
  json_free(v);

  PASS();
}

static void test_parse_nested(void) {
  TEST("parse nested");

  const char *json_str =
      "{\"users\": [{\"id\": 1, \"name\": \"Alice\"}, {\"id\": 2, \"name\": "
      "\"Bob\"}]}";
  json_value_t *v = json_parse(json_str, NULL);
  ASSERT(v != NULL, "Parse failed");

  json_value_t *users = json_object_get(v, "users");
  ASSERT(json_is_array(users), "users not array");
  ASSERT(json_array_len(users) == 2, "Wrong user count");

  json_value_t *user0 = json_array_get(users, 0);
  json_value_t *name0 = json_object_get(user0, "name");
  ASSERT(strcmp(json_get_string(name0), "Alice") == 0, "Wrong first user");

  json_free(v);
  PASS();
}

/* ========== Stringify tests ========== */

static void test_stringify_primitives(void) {
  TEST("stringify primitives");

  char *s = json_stringify(json_null(), false);
  ASSERT(strcmp(s, "null") == 0, "null stringify");
  free(s);

  json_value_t *v = json_bool(true);
  s = json_stringify(v, false);
  ASSERT(strcmp(s, "true") == 0, "true stringify");
  free(s);
  json_free(v);

  v = json_number(42);
  s = json_stringify(v, false);
  ASSERT(strcmp(s, "42") == 0, "number stringify");
  free(s);
  json_free(v);

  v = json_string("hello");
  s = json_stringify(v, false);
  ASSERT(strcmp(s, "\"hello\"") == 0, "string stringify");
  free(s);
  json_free(v);

  PASS();
}

static void test_stringify_roundtrip(void) {
  TEST("stringify roundtrip");

  const char *original = "{\"name\":\"John\",\"age\":30,\"active\":true}";
  json_value_t *v = json_parse(original, NULL);
  ASSERT(v != NULL, "Parse failed");

  char *s = json_stringify(v, false);
  ASSERT(s != NULL, "Stringify failed");

  /* Parse again and compare */
  json_value_t *v2 = json_parse(s, NULL);
  ASSERT(v2 != NULL, "Re-parse failed");

  ASSERT(strcmp(json_get_string(json_object_get(v2, "name")), "John") == 0,
         "Roundtrip name mismatch");
  ASSERT(json_get_number(json_object_get(v2, "age")) == 30,
         "Roundtrip age mismatch");

  free(s);
  json_free(v);
  json_free(v2);
  PASS();
}

/* ========== Error handling tests ========== */

static void test_parse_errors(void) {
  TEST("parse errors");

  json_ctx_t ctx;
  json_value_t *v;

  /* Invalid JSON */
  v = json_parse("invalid", &ctx);
  ASSERT(v == NULL, "Should fail on invalid");
  ASSERT(ctx.error != JSON_OK, "Error not set");

  /* Unterminated string */
  v = json_parse("\"hello", &ctx);
  ASSERT(v == NULL, "Should fail on unterminated string");

  /* Trailing comma in array */
  v = json_parse("[1, 2, ]", &ctx);
  ASSERT(v == NULL, "Should fail on trailing comma");

  /* Trailing content */
  v = json_parse("123 extra", &ctx);
  ASSERT(v == NULL, "Should fail on trailing content");

  PASS();
}

/* ========== Builder tests ========== */

static void test_build_array(void) {
  TEST("build array");

  json_value_t *arr = json_array_new();
  ASSERT(json_array_push(arr, json_number(1)), "Push 1 failed");
  ASSERT(json_array_push(arr, json_number(2)), "Push 2 failed");
  ASSERT(json_array_push(arr, json_string("three")), "Push 3 failed");

  ASSERT(json_array_len(arr) == 3, "Wrong length");

  char *s = json_stringify(arr, false);
  ASSERT(strstr(s, "1") != NULL, "Missing 1");
  ASSERT(strstr(s, "2") != NULL, "Missing 2");
  ASSERT(strstr(s, "\"three\"") != NULL, "Missing three");

  free(s);
  json_free(arr);
  PASS();
}

static void test_build_object(void) {
  TEST("build object");

  json_value_t *obj = json_object_new();
  ASSERT(json_object_set(obj, "name", json_string("Alice")), "Set name failed");
  ASSERT(json_object_set(obj, "age", json_number(25)), "Set age failed");
  ASSERT(json_object_set(obj, "active", json_bool(true)), "Set active failed");

  ASSERT(json_object_len(obj) == 3, "Wrong key count");
  ASSERT(json_object_has(obj, "name"), "Missing name");
  ASSERT(!json_object_has(obj, "missing"), "Has missing key");

  /* Overwrite */
  ASSERT(json_object_set(obj, "age", json_number(26)), "Overwrite failed");
  ASSERT(json_get_number(json_object_get(obj, "age")) == 26,
         "Overwrite not applied");

  json_free(obj);
  PASS();
}

/* ========== Main ========== */

int main(void) {
  printf("\n==================================\n");
  printf("  JSON Parser Test Suite\n");
  printf("==================================\n\n");

  printf("Primitive Parsing Tests:\n");
  test_parse_null();
  test_parse_bool();
  test_parse_number();
  test_parse_string();

  printf("\nComposite Type Tests:\n");
  test_parse_array();
  test_parse_object();
  test_parse_nested();

  printf("\nStringify Tests:\n");
  test_stringify_primitives();
  test_stringify_roundtrip();

  printf("\nError Handling Tests:\n");
  test_parse_errors();

  printf("\nBuilder Tests:\n");
  test_build_array();
  test_build_object();

  printf("\n==================================\n");
  printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
  printf("==================================\n\n");

  return tests_passed == tests_run ? 0 : 1;
}

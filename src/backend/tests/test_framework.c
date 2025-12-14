/**
 * Web Framework Test Suite
 */
#include "../src/framework/framework.h"
#include "../src/json/json.h"

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

/* ========== Route matching tests ========== */

static void test_route_match_static(void) {
  TEST("route match static");

  kfw_request_t req;
  kfw_request_init(&req);

  ASSERT(kfw_match_route("/api/blogs", "/api/blogs", &req),
         "Static match failed");
  ASSERT(!kfw_match_route("/api/blogs", "/api/users", &req),
         "Should not match");
  ASSERT(!kfw_match_route("/api/blogs", "/api/blogs/extra", &req),
         "Extra segment");

  kfw_request_cleanup(&req);
  PASS();
}

static void test_route_match_params(void) {
  TEST("route match params");

  kfw_request_t req;
  kfw_request_init(&req);

  ASSERT(kfw_match_route("/api/blog/:slug", "/api/blog/hello-world", &req),
         "Param match failed");
  ASSERT(req.path_params_count == 1, "Wrong param count");
  ASSERT(strcmp(kfw_req_param(&req, "slug"), "hello-world") == 0,
         "Wrong param value");

  kfw_request_cleanup(&req);

  /* Multiple params */
  kfw_request_init(&req);
  ASSERT(kfw_match_route("/api/:type/:id", "/api/posts/123", &req),
         "Multi-param match failed");
  ASSERT(req.path_params_count == 2, "Wrong multi-param count");
  ASSERT(strcmp(kfw_req_param(&req, "type"), "posts") == 0, "Wrong type param");
  ASSERT(strcmp(kfw_req_param(&req, "id"), "123") == 0, "Wrong id param");

  kfw_request_cleanup(&req);
  PASS();
}

static void test_route_match_url_decode(void) {
  TEST("route match URL decode");

  kfw_request_t req;
  kfw_request_init(&req);

  ASSERT(kfw_match_route("/api/blog/:slug", "/api/blog/hello%20world", &req),
         "URL encoded match failed");
  ASSERT(strcmp(kfw_req_param(&req, "slug"), "hello world") == 0,
         "URL decode failed");

  kfw_request_cleanup(&req);
  PASS();
}

/* ========== Query string tests ========== */

static void test_query_string_parse(void) {
  TEST("query string parse");

  char *keys[16];
  char *values[16];

  int count = kfw_parse_query_string("foo=bar&baz=qux", keys, values, 16);
  ASSERT(count == 2, "Wrong pair count");
  ASSERT(strcmp(keys[0], "foo") == 0, "Wrong key 0");
  ASSERT(strcmp(values[0], "bar") == 0, "Wrong value 0");
  ASSERT(strcmp(keys[1], "baz") == 0, "Wrong key 1");
  ASSERT(strcmp(values[1], "qux") == 0, "Wrong value 1");

  for (int i = 0; i < count; i++) {
    free(keys[i]);
    free(values[i]);
  }

  /* URL encoded */
  count =
      kfw_parse_query_string("name=hello%20world&val=a%2Bb", keys, values, 16);
  ASSERT(count == 2, "Encoded count wrong");
  ASSERT(strcmp(values[0], "hello world") == 0, "Space decode failed");
  ASSERT(strcmp(values[1], "a+b") == 0, "Plus decode failed");

  for (int i = 0; i < count; i++) {
    free(keys[i]);
    free(values[i]);
  }

  PASS();
}

/* ========== Handler tests ========== */

static kfw_error_t test_handler_ok(kfw_request_t *req, kfw_response_t *resp) {
  (void)req;
  kfw_resp_text(resp, 200, "OK");
  return KFW_OK;
}

static kfw_error_t test_handler_json(kfw_request_t *req, kfw_response_t *resp) {
  (void)req;
  json_value_t *obj = json_object_new();
  json_object_set(obj, "message", json_string("Hello, World!"));
  kfw_resp_json(resp, obj);
  json_free(obj);
  return KFW_OK;
}

static kfw_error_t test_handler_param(kfw_request_t *req,
                                      kfw_response_t *resp) {
  const char *id = kfw_req_param(req, "id");
  if (!id) {
    kfw_resp_error(resp, 400, "Missing id");
    return KFW_ERR_BAD_REQUEST;
  }

  json_value_t *obj = json_object_new();
  json_object_set(obj, "id", json_string(id));
  kfw_resp_json(resp, obj);
  json_free(obj);
  return KFW_OK;
}

static kfw_error_t test_handler_body(kfw_request_t *req, kfw_response_t *resp) {
  if (!req->json) {
    kfw_resp_error(resp, 400, "Missing JSON body");
    return KFW_ERR_BAD_REQUEST;
  }

  json_value_t *name = kfw_req_json_get(req, "name");
  if (!name || !json_is_string(name)) {
    kfw_resp_error(resp, 400, "Missing name field");
    return KFW_ERR_BAD_REQUEST;
  }

  json_value_t *obj = json_object_new();
  json_object_set(obj, "greeting", json_string(json_get_string(name)));
  kfw_resp_json(resp, obj);
  json_free(obj);
  return KFW_OK;
}

static void test_app_basic(void) {
  TEST("app basic handler");

  kfw_app_t *app = kfw_app_create();
  ASSERT(app != NULL, "App create failed");

  ASSERT(KFW_GET(app, "/api/test", test_handler_ok), "Route register failed");

  http_request_t http_req = {
      .method = "GET", .path = "/api/test", .keep_alive = true};
  http_response_t http_resp = {0};

  kfw_error_t err = kfw_handle(app, &http_req, NULL, 0, &http_resp);
  ASSERT(err == KFW_OK, "Handle failed");
  ASSERT(http_resp.status_code == 200, "Wrong status");
  ASSERT(strcmp(http_resp.body, "OK") == 0, "Wrong body");

  free(http_resp.body);
  free((void *)http_resp.content_type);
  kfw_app_destroy(app);
  PASS();
}

static void test_app_json_response(void) {
  TEST("app JSON response");

  kfw_app_t *app = kfw_app_create();
  KFW_GET(app, "/api/hello", test_handler_json);

  http_request_t http_req = {.method = "GET", .path = "/api/hello"};
  http_response_t http_resp = {0};

  kfw_handle(app, &http_req, NULL, 0, &http_resp);

  ASSERT(http_resp.status_code == 200, "Wrong status");
  ASSERT(strstr(http_resp.content_type, "application/json") != NULL,
         "Wrong content type");

  json_value_t *json = json_parse(http_resp.body, NULL);
  ASSERT(json != NULL, "Response not valid JSON");
  ASSERT(json_object_has(json, "message"), "Missing message field");

  free(http_resp.body);
  free((void *)http_resp.content_type);
  json_free(json);
  kfw_app_destroy(app);
  PASS();
}

static void test_app_path_params(void) {
  TEST("app path params");

  kfw_app_t *app = kfw_app_create();
  KFW_GET(app, "/api/item/:id", test_handler_param);

  http_request_t http_req = {.method = "GET", .path = "/api/item/42"};
  http_response_t http_resp = {0};

  kfw_handle(app, &http_req, NULL, 0, &http_resp);

  ASSERT(http_resp.status_code == 200, "Wrong status");

  json_value_t *json = json_parse(http_resp.body, NULL);
  ASSERT(strcmp(json_get_string(json_object_get(json, "id")), "42") == 0,
         "Wrong id in response");

  free(http_resp.body);
  free((void *)http_resp.content_type);
  json_free(json);
  kfw_app_destroy(app);
  PASS();
}

static void test_app_post_body(void) {
  TEST("app POST body");

  kfw_app_t *app = kfw_app_create();
  KFW_POST(app, "/api/greet", test_handler_body);

  http_request_t http_req = {.method = "POST", .path = "/api/greet"};
  http_response_t http_resp = {0};

  const char *body = "{\"name\": \"Alice\"}";
  kfw_handle(app, &http_req, body, strlen(body), &http_resp);

  ASSERT(http_resp.status_code == 200, "Wrong status");

  json_value_t *json = json_parse(http_resp.body, NULL);
  ASSERT(strcmp(json_get_string(json_object_get(json, "greeting")), "Alice") ==
             0,
         "Wrong greeting");

  free(http_resp.body);
  free((void *)http_resp.content_type);
  json_free(json);
  kfw_app_destroy(app);
  PASS();
}

static void test_app_not_found(void) {
  TEST("app 404 not found");

  kfw_app_t *app = kfw_app_create();
  KFW_GET(app, "/api/exists", test_handler_ok);

  http_request_t http_req = {.method = "GET", .path = "/api/missing"};
  http_response_t http_resp = {0};

  kfw_handle(app, &http_req, NULL, 0, &http_resp);

  ASSERT(http_resp.status_code == 404, "Wrong status for 404");

  free(http_resp.body);
  free((void *)http_resp.content_type);
  kfw_app_destroy(app);
  PASS();
}

static void test_app_method_not_allowed(void) {
  TEST("app 405 method not allowed");

  kfw_app_t *app = kfw_app_create();
  KFW_GET(app, "/api/readonly", test_handler_ok);

  http_request_t http_req = {.method = "POST", .path = "/api/readonly"};
  http_response_t http_resp = {0};

  kfw_handle(app, &http_req, NULL, 0, &http_resp);

  ASSERT(http_resp.status_code == 405, "Wrong status for 405");

  free(http_resp.body);
  free((void *)http_resp.content_type);
  kfw_app_destroy(app);
  PASS();
}

/* ========== Main ========== */

int main(void) {
  printf("\n==================================\n");
  printf("  Web Framework Test Suite\n");
  printf("==================================\n\n");

  printf("Route Matching Tests:\n");
  test_route_match_static();
  test_route_match_params();
  test_route_match_url_decode();

  printf("\nQuery String Tests:\n");
  test_query_string_parse();

  printf("\nHandler Tests:\n");
  test_app_basic();
  test_app_json_response();
  test_app_path_params();
  test_app_post_body();
  test_app_not_found();
  test_app_method_not_allowed();

  printf("\n==================================\n");
  printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
  printf("==================================\n\n");

  return tests_passed == tests_run ? 0 : 1;
}

#!/bin/bash

BASE_URL="http://localhost:3000"
TOKEN="da5c2bc3-45ba-db12-7c65-d6f1fff55719"

echo "Starting E2E CRUD Test (Bash/Curl)..."

# 1. Create Blog
echo "[1] Creating Blog Post..."
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" -X POST "$BASE_URL/api/admin/blog" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Test Blog Post",
    "url_slug": "test-blog-post",
    "description": "A test post",
    "content": "# Hello World\nThis is a test.",
    "status": 1,
    "category": "Tech",
    "tags": "test,e2e"
  }')

if [ "$RESPONSE" != "201" ]; then
  echo "FAILED: Create blog failed with $RESPONSE"
  exit 1
fi
echo "SUCCESS: Blog created."

# 2. Update Blog
echo "[2] Updating Blog Post..."
RESPONSE=$(curl -s -X PUT "$BASE_URL/api/admin/blog/test-blog-post" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Test Blog Post UPDATED",
    "url_slug": "test-blog-post",
    "description": "A test post updated",
    "content": "# Hello World\nThis is a test updated.",
    "status": 1,
    "category": "Tech",
    "tags": "test,e2e,updated"
  }')

# Check if title is updated in response
if [[ "$RESPONSE" != *"Test Blog Post UPDATED"* ]]; then
  echo "FAILED: Update did not persist title. Got response: $RESPONSE"
  exit 1
fi
echo "SUCCESS: Blog updated."

# 3. Read Blog (Public)
echo "[3] Reading Blog Post (Public)..."
RESPONSE=$(curl -s "$BASE_URL/api/blog/test-blog-post")
if [[ "$RESPONSE" != *"This is a test updated."* ]]; then
  echo "FAILED: Content mismatch on read. Got: $RESPONSE"
  exit 1
fi
echo "SUCCESS: Blog read correctly."

# 4. Delete Blog
echo "[4] Deleting Blog Post..."
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/api/admin/blog/test-blog-post" \
  -H "Authorization: Bearer $TOKEN")

if [ "$RESPONSE" != "200" ]; then
  echo "FAILED: Delete blog failed with $RESPONSE"
  exit 1
fi
echo "SUCCESS: Blog deleted."

# 5. Verify Deletion
echo "[5] Verifying Deletion..."
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/api/blog/test-blog-post")
if [ "$RESPONSE" != "404" ]; then
  echo "FAILED: Blog still exists after delete (Got $RESPONSE)"
  exit 1
fi
echo "SUCCESS: Blog verified deleted."

echo "ALL TESTS PASSED"
exit 0

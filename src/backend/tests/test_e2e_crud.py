import requests
import sys
import time
import json

BASE_URL = "http://localhost:3000"
TOKEN = "272f78eb-d498-3dbb-eb42-a30909cb7aca"

headers = {
    "Authorization": f"Bearer {TOKEN}",
    "Content-Type": "application/json"
}

def test_crud():
    print("Starting E2E CRUD Test...")
    
    # 1. Create Blog
    print("[1] Creating Blog Post...")
    post_data = {
        "title": "Test Blog Post",
        "url_slug": "test-blog-post",
        "description": "A test post",
        "content": "# Hello World\nThis is a test.",
        "status": 1, # Published
        "category": "Tech",
        "tags": "test,e2e"
    }
    
    try:
        res = requests.post(f"{BASE_URL}/api/admin/blog", json=post_data, headers=headers)
        if res.status_code != 201:
            print(f"FAILED: Create blog failed with {res.status_code}: {res.text}")
            return False
        print("SUCCESS: Blog created.")
        
        # 2. Update Blog (The Fix verification)
        print("[2] Updating Blog Post...")
        update_data = {
            "title": "Test Blog Post UPDATED",
            "url_slug": "test-blog-post", # Same slug
            "description": "A test post updated",
            "content": "# Hello World\nThis is a test updated.",
            "status": 1,
            "category": "Tech",
            "tags": "test,e2e,updated"
        }
        res = requests.put(f"{BASE_URL}/api/admin/blog/test-blog-post", json=update_data, headers=headers)
        if res.status_code != 200:
            print(f"FAILED: Update blog failed with {res.status_code}: {res.text}")
            return False
            
        verify_json = res.json()
        if verify_json.get("title") != "Test Blog Post UPDATED":
            print(f"FAILED: Update did not persist title. Got: {verify_json.get('title')}")
            return False
        print("SUCCESS: Blog updated.")
        
        # 3. Read Blog (Public)
        print("[3] Reading Blog Post (Public)...")
        res = requests.get(f"{BASE_URL}/api/blog/test-blog-post")
        if res.status_code != 200:
            print(f"FAILED: Read blog failed with {res.status_code}")
            return False
        
        public_json = res.json()
        if public_json.get("content") != "# Hello World\nThis is a test updated.":
             print(f"FAILED: Content mismatch on read.")
             return False
        print("SUCCESS: Blog read correctly.")
        
        # 4. Delete Blog
        print("[4] Deleting Blog Post...")
        res = requests.delete(f"{BASE_URL}/api/admin/blog/test-blog-post", headers=headers)
        if res.status_code != 200:
             print(f"FAILED: Delete blog failed with {res.status_code}")
             return False
        print("SUCCESS: Blog deleted.")
        
        # 5. Verify Deletion
        res = requests.get(f"{BASE_URL}/api/blog/test-blog-post")
        if res.status_code != 404:
             print(f"FAILED: Blog still exists after delete (Got {res.status_code})")
             return False
        print("SUCCESS: Blog verified deleted.")
        
        return True
        
    except Exception as e:
        print(f"EXCEPTION: {e}")
        return False

if __name__ == "__main__":
    if test_crud():
        print("\nALL TESTS PASSED")
        sys.exit(0)
    else:
        print("\nTESTS FAILED")
        sys.exit(1)

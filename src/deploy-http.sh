#!/bin/bash
set -e

echo "### Stopping any existing containers..."
docker compose down
docker stop kherashanu-nginx-temp 2>/dev/null || true
docker rm kherashanu-nginx-temp 2>/dev/null || true

echo "### Starting in HTTP-only mode..."
docker compose -f docker-compose-http.yml up -d

echo "### Verifying deployment..."
docker ps

echo "### Done! Site should be accessible at http://kherashanu.com"
echo "NOTE: Google OAuth requires HTTPS. You may need to add 'http://kherashanu.com' to your Google Cloud Console Authorized Javascript Origins and Redirect URIs."

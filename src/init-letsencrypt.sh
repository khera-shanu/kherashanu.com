#!/bin/bash

# Initialize Let's Encrypt SSL certificates for kherashanu.com
# This script should be run once before starting the full docker compose stack

set -e

DOMAIN="kherashanu.com"
EMAIL="kherashanu@gmail.com"
# IMPORTANT: Set STAGING=0 for production certificates after testing
# STAGING=1 uses Let's Encrypt staging environment (for testing, no rate limits)
# STAGING=0 uses Let's Encrypt production environment (real certificates, rate limited)
STAGING=0

echo "### Preparing Let's Encrypt certificate for $DOMAIN..."

# Create necessary directories
echo "### Creating required directories..."
mkdir -p ./certbot/conf
mkdir -p ./certbot/www

# Download recommended TLS parameters
if [ ! -f "./certbot/conf/options-ssl-nginx.conf" ]; then
  echo "### Downloading recommended TLS parameters..."
  curl -s https://raw.githubusercontent.com/certbot/certbot/master/certbot-nginx/certbot_nginx/_internal/tls_configs/options-ssl-nginx.conf > ./certbot/conf/options-ssl-nginx.conf
fi

if [ ! -f "./certbot/conf/ssl-dhparams.pem" ]; then
  echo "### Downloading recommended TLS DH parameters..."
  curl -s https://raw.githubusercontent.com/certbot/certbot/master/certbot/certbot/ssl-dhparams.pem > ./certbot/conf/ssl-dhparams.pem
fi

# Create dummy certificate for nginx to start
if [ ! -d "./certbot/conf/live/$DOMAIN" ]; then
  echo "### Switching to HTTP-only nginx config for certificate acquisition..."
  # Temporarily use HTTP-only config (no SSL requirements)
  cp nginx-ssl.conf nginx-ssl.conf.bak
  cp nginx-http-only.conf nginx.conf.tmp
fi

# Start nginx with HTTP-only config via temporary override
echo "### Starting application and nginx (HTTP-only mode)..."

# Stop any running nginx from docker compose
docker compose stop nginx 2>/dev/null || true
docker compose rm -f nginx 2>/dev/null || true

# Clean up any existing temporary nginx container
docker stop kherashanu-nginx-temp 2>/dev/null || true
docker rm kherashanu-nginx-temp 2>/dev/null || true

# Start application container first (this creates the network)
docker compose up -d kherashanu

echo "### Waiting for network to be ready..."
sleep 2

# Now start nginx with HTTP-only config (network exists now)
# Important: Use Docker volumes WITH src_ prefix to match docker compose volume names
docker run -d --name kherashanu-nginx-temp \
  --network src_kherashanu-network \
  -p 80:80 \
  -v "$(pwd)/nginx-http-only.conf:/etc/nginx/nginx.conf:ro" \
  -v "$(pwd)/certbot/conf:/etc/letsencrypt:ro" \
  -v "$(pwd)/certbot/www:/var/www/certbot:rw" \
  nginx:alpine

# Ensure the ACME challenge directory exists with proper permissions
docker exec kherashanu-nginx-temp mkdir -p /var/www/certbot/.well-known/acme-challenge
docker exec kherashanu-nginx-temp chmod -R 755 /var/www/certbot

echo "### Waiting for nginx to be ready..."
sleep 5

echo "### Deleting any existing certificates for $DOMAIN..."
docker compose run --rm --entrypoint "\
  rm -rf /etc/letsencrypt/live/$DOMAIN && \
  rm -rf /etc/letsencrypt/archive/$DOMAIN && \
  rm -rf /etc/letsencrypt/renewal/$DOMAIN.conf" certbot || true

echo "### Requesting Let's Encrypt certificate for $DOMAIN..."

# Select appropriate certificate mode
if [ $STAGING != "0" ]; then
  STAGING_ARG="--staging"
  echo "### Using STAGING environment (for testing)..."
else
  STAGING_ARG=""
  echo "### Using PRODUCTION environment..."
fi

# Request certificate
docker compose run --rm --entrypoint "\
  certbot certonly --webroot -w /var/www/certbot \
    $STAGING_ARG \
    --email $EMAIL \
    --agree-tos \
    --no-eff-email \
    -d $DOMAIN" certbot

echo "### Certificate obtained successfully!"

echo "### Stopping temporary HTTP-only nginx..."
docker stop kherashanu-nginx-temp
docker rm kherashanu-nginx-temp

echo "### Starting full stack with SSL..."
docker compose up -d

echo "### Waiting for services to start..."
sleep 3

echo "### Reloading nginx with SSL configuration..."
docker compose exec nginx nginx -s reload || echo "Nginx will auto-reload on next restart"

echo "### Certificate setup complete!"
echo "### To switch to production certificates, set STAGING=0 in this script and run again."

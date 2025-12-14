#!/bin/bash

# Initialize Let's Encrypt SSL certificates for kherashanu.com
# This script should be run once before starting the full docker-compose stack

set -e

DOMAIN="kherashanu.com"
EMAIL="kherashanu@gmail.com"
# IMPORTANT: Set STAGING=0 for production certificates after testing
# STAGING=1 uses Let's Encrypt staging environment (for testing, no rate limits)
# STAGING=0 uses Let's Encrypt production environment (real certificates, rate limited)
STAGING=1

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
  echo "### Creating dummy certificate for $DOMAIN..."
  mkdir -p "./certbot/conf/live/$DOMAIN"
  docker compose run --rm --entrypoint "\
    openssl req -x509 -nodes -newkey rsa:4096 -days 1\
      -keyout /etc/letsencrypt/live/$DOMAIN/privkey.pem \
      -out /etc/letsencrypt/live/$DOMAIN/fullchain.pem \
      -subj '/CN=localhost'" certbot
fi

echo "### Starting application and nginx..."
docker compose up -d kherashanu nginx

echo "### Waiting for application to be ready..."
sleep 5

echo "### Deleting dummy certificate for $DOMAIN..."
docker compose run --rm --entrypoint "\
  rm -rf /etc/letsencrypt/live/$DOMAIN && \
  rm -rf /etc/letsencrypt/archive/$DOMAIN && \
  rm -rf /etc/letsencrypt/renewal/$DOMAIN.conf" certbot

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

echo "### Reloading nginx..."
docker compose exec nginx nginx -s reload

echo "### Certificate setup complete!"
echo "### To switch to production certificates, set STAGING=0 in this script and run again."

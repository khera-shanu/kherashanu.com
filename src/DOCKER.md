# Docker Compose Usage Guide

## Quick Start

### 1. Setup Environment
```bash
# Copy environment template
cp .env.example .env

# Edit .env with your settings
nano .env
```

### 2. Build and Start
```bash
# Build and start in detached mode
docker-compose up -d --build

# Or just start (if already built)
docker-compose up -d
```

### 3. View Logs
```bash
# Follow all logs
docker-compose logs -f

# Follow specific service
docker-compose logs -f kherashanu
```

### 4. Stop Services
```bash
# Stop containers (preserves data)
docker-compose stop

# Stop and remove containers (preserves volumes)
docker-compose down

# Stop, remove containers and volumes (⚠️ deletes data)
docker-compose down -v
```

## Common Operations

### Rebuild After Code Changes
```bash
docker-compose up -d --build
```

### Restart Service
```bash
docker-compose restart kherashanu
```

### View Service Status
```bash
docker-compose ps
```

### Execute Commands in Container
```bash
# Get shell access
docker-compose exec kherashanu /bin/bash

# Run one-off command
docker-compose exec kherashanu ls -la /app/data
```

### Scale Services (if needed)
```bash
docker-compose up -d --scale kherashanu=3
```

## Production Deployment

### 1. Enable Nginx Reverse Proxy
Uncomment the nginx service in `docker-compose.yml` and create `nginx.conf`:

```nginx
events {
    worker_connections 1024;
}

http {
    upstream kherashanu {
        server kherashanu:3000;
    }

    server {
        listen 80;
        server_name yourdomain.com;

        location / {
            proxy_pass http://kherashanu;
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
        }
    }
}
```

### 2. SSL/TLS Setup with Certbot

The project includes automated SSL certificate management using Let's Encrypt and Certbot.

#### Initial Certificate Setup

**Option 1: Using the initialization script (Recommended)**

```bash
# Edit init-letsencrypt.sh to configure your domain and email
nano init-letsencrypt.sh

# Set variables:
# DOMAIN="kherashanu.com"
# EMAIL="your@email.com"
# STAGING=1  # Use 1 for testing, 0 for production

# Make it executable (if not already)
chmod +x init-letsencrypt.sh

# Run the script
./init-letsencrypt.sh
```

**Option 2: Manual setup**

```bash
# Start nginx first
docker-compose up -d nginx

# Obtain staging certificate (for testing - has no rate limits)
docker-compose run --rm certbot certonly --webroot \
  -w /var/www/certbot \
  --staging \
  --email your@email.com \
  --agree-tos \
  --no-eff-email \
  -d kherashanu.com \
  -d www.kherashanu.com

# Reload nginx
docker-compose exec nginx nginx -s reload

# Once satisfied, get production certificate
docker-compose run --rm certbot certonly --webroot \
  -w /var/www/certbot \
  --email your@email.com \
  --agree-tos \
  --no-eff-email \
  --force-renewal \
  -d kherashanu.com \
  -d www.kherashanu.com

# Reload nginx again
docker-compose exec nginx nginx -s reload
```

#### Certificate Renewal

Certificates are automatically renewed by the running certbot container, which checks twice daily and renews certificates that are within 30 days of expiration.

**Manual renewal (if needed):**
```bash
# Renew all certificates
docker-compose run --rm certbot renew

# Reload nginx to use new certificates
docker-compose exec nginx nginx -s reload
```

#### Switching from Staging to Production

```bash
# Remove staging certificates
docker-compose run --rm --entrypoint "\
  rm -rf /etc/letsencrypt/live/kherashanu.com && \
  rm -rf /etc/letsencrypt/archive/kherashanu.com && \
  rm -rf /etc/letsencrypt/renewal/kherashanu.com.conf" certbot

# Obtain production certificate
docker-compose run --rm certbot certonly --webroot \
  -w /var/www/certbot \
  --email your@email.com \
  --agree-tos \
  --no-eff-email \
  -d kherashanu.com \
  -d www.kherashanu.com

# Reload nginx
docker-compose exec nginx nginx -s reload
```

#### Verifying Certificate

```bash
# Check certificate expiration
docker-compose run --rm certbot certificates

# Test SSL configuration
curl -vI https://kherashanu.com

# Check SSL rating (after DNS is configured)
# Visit: https://www.ssllabs.com/ssltest/analyze.html?d=kherashanu.com
```


### 3. Environment Variables
Create `.env` file:
```bash
ADMIN_EMAIL=your@email.com
GOOGLE_REDIRECT_URI=https://yourdomain.com/auth/callback
```

### 4. Mount OAuth Secrets
Uncomment the google_oauth_secrets volume mount in `docker-compose.yml`:
```yaml
volumes:
  - ./google_oauth_secrets.json:/app/google_oauth_secrets.json:ro
```

## Backup and Restore

### Backup Database
```bash
# Create backup directory
mkdir -p backups

# Copy database
docker-compose exec kherashanu cp /app/data/blog.db /app/data/blog.db.backup
docker cp kherashanu:/app/data/blog.db.backup ./backups/blog-$(date +%Y%m%d-%H%M%S).db
```

### Restore Database
```bash
docker cp ./backups/blog-20231214.db kherashanu:/app/data/blog.db
docker-compose restart kherashanu
```

## Troubleshooting

### Check Container Health
```bash
docker-compose ps
docker inspect kherashanu | grep -A 10 Health
```

### View Resource Usage
```bash
docker stats kherashanu
```

### Reset Everything
```bash
docker-compose down -v
docker-compose up -d --build
```

### Check Network
```bash
docker network ls
docker network inspect kherashanu_kherashanu-network
```

## Monitoring

### Health Check Status
```bash
watch -n 5 'docker inspect kherashanu | grep -A 5 Health'
```

### Log Rotation (Production)
Add to docker-compose.yml:
```yaml
logging:
  driver: "json-file"
  options:
    max-size: "10m"
    max-file: "3"
```

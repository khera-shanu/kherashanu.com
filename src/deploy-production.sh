#!/bin/bash

# Production deployment script for kherashanu.com
# This script handles the complete production deployment process

set -e

echo "🚀 Starting production deployment for kherashanu.com..."

# Check if .env file exists
if [ ! -f .env ]; then
    echo "⚠️  No .env file found. Creating from .env.example..."
    cp .env.example .env
    echo "✅ Created .env file - please review and customize if needed"
else
    echo "✅ Found existing .env file"
fi

# Check if DNS is configured
echo ""
echo "🔍 Checking DNS configuration..."
if host kherashanu.com > /dev/null 2>&1; then
    echo "✅ DNS configured for kherashanu.com"
    host kherashanu.com
else
    echo "⚠️  Warning: DNS not configured for kherashanu.com"
    echo "   Please ensure your domain points to this server's IP"
    read -p "   Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check if certificates already exist
echo ""
# Check if certificates actually exist in the docker volume
echo ""
echo "🔍 Checking for existing SSL certificates..."
CERT_EXISTS=false
if docker compose run --rm --entrypoint /bin/sh certbot -c "test -f /etc/letsencrypt/live/kherashanu.com/fullchain.pem" >/dev/null 2>&1; then
    CERT_EXISTS=true
fi

if [ "$CERT_EXISTS" = "true" ]; then
    echo "✅ SSL certificates found in volume"
    read -p "   Re-initialize certificates anyway? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🔐 Running SSL certificate initialization..."
        chmod +x init-letsencrypt.sh
        ./init-letsencrypt.sh
    else
        echo "⏭️  Skipping initialization (using existing certificates)"
    fi
else
    echo "⚠️  SSL certificates NOT found in volume"
    echo "🔐 Initializing SSL certificates..."
    if [ ! -f ./init-letsencrypt.sh ]; then
        echo "❌ init-letsencrypt.sh not found!"
        exit 1
    fi
    chmod +x init-letsencrypt.sh
    ./init-letsencrypt.sh
fi

# Build and start services
echo ""
echo "🐳 Building Docker images..."
docker compose build

echo ""
echo "🚀 Starting services..."
docker compose up -d

# Wait for services to be healthy
echo ""
echo "⏳ Waiting for services to be healthy..."
sleep 5

# Check service status
echo ""
echo "📊 Service Status:"
docker compose ps

# Check health
echo ""
echo "🏥 Health Check:"
if docker inspect kherashanu 2>/dev/null | grep -q '"Status": "healthy"'; then
    echo "✅ Application is healthy"
else
    echo "⏳ Application is starting... (health check in progress)"
fi

# Test endpoints
echo ""
echo "🧪 Testing endpoints..."
sleep 2

if curl -f -k https://localhost > /dev/null 2>&1; then
    echo "✅ HTTPS endpoint responding"
else
    echo "⚠️  HTTPS endpoint not yet responding (may need more time)"
fi

if curl -f http://localhost 2>&1 | grep -q "301\|302\|Moved"; then
    echo "✅ HTTP to HTTPS redirect working"
else
    echo "⚠️  HTTP redirect not yet configured"
fi

# Final status
echo ""
echo "═══════════════════════════════════════════════════════"
echo "🎉 Deployment complete!"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "Your application is now running at:"
echo "  🌐 https://kherashanu.com"
echo ""
echo "Next steps:"
echo "  1. Verify the site loads in your browser"
echo "  2. Test Google OAuth authentication"
echo "  3. Check certificate status: docker compose run --rm certbot certificates"
echo "  4. Monitor logs: docker compose logs -f"
echo ""
echo "For troubleshooting, see PRODUCTION_CHECKLIST.md"
echo "═══════════════════════════════════════════════════════"

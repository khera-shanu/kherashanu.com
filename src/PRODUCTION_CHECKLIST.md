# Production Deployment Checklist for kherashanu.com

## Pre-Deployment

- [ ] **DNS Configuration**
  - Point `kherashanu.com` A record to your server's public IP
  - Point `www.kherashanu.com` A record to your server's public IP
  - Verify DNS propagation: `dig kherashanu.com`

- [ ] **Server Requirements**
  - Ubuntu/Debian server with Docker & Docker Compose installed
  - Ports 80 (HTTP) and 443 (HTTPS) open in firewall
  - At least 2GB RAM, 20GB disk space recommended

- [ ] **Environment Configuration**
  - Copy `.env.example` to `.env`
  - Verify `ADMIN_EMAIL=kherashanu@gmail.com`
  - Verify `GOOGLE_REDIRECT_URI=https://kherashanu.com/auth/callback`
  - Update Google Cloud Console OAuth credentials with production redirect URI

## SSL Certificate Setup

- [ ] **Test with Staging Certificates First**
  ```bash
  # Edit init-letsencrypt.sh to ensure STAGING=1
  nano init-letsencrypt.sh
  
  # Run initialization
  ./init-letsencrypt.sh
  
  # Verify staging certificate obtained
  docker-compose run --rm certbot certificates
  ```

- [ ] **Switch to Production Certificates**
  ```bash
  # Edit init-letsencrypt.sh and set STAGING=0
  nano init-letsencrypt.sh
  
  # Re-run initialization
  ./init-letsencrypt.sh
  ```

- [ ] **Verify SSL Configuration**
  ```bash
  # Test HTTPS
  curl -vI https://kherashanu.com
  
  # Check certificate details
  docker-compose run --rm certbot certificates
  
  # Test SSL rating (optional)
  # Visit: https://www.ssllabs.com/ssltest/analyze.html?d=kherashanu.com
  ```

## Deployment

- [ ] **Build and Start Services**
  ```bash
  # Build application
  docker-compose build
  
  # Start all services
  docker-compose up -d
  
  # Verify all containers running
  docker-compose ps
  ```

- [ ] **Health Checks**
  ```bash
  # Check container health
  docker-compose ps
  docker inspect kherashanu | grep -A 5 Health
  
  # View logs
  docker-compose logs -f
  ```

- [ ] **Functional Testing**
  - Visit `https://kherashanu.com`
  - Verify homepage loads correctly
  - Test blog functionality
  - Test portfolio section
  - Test Google OAuth login
  - Test admin dashboard (after authentication)

## Post-Deployment

- [ ] **Security Verification**
  - Verify HTTPS redirect works: `curl -I http://kherashanu.com`
  - Check security headers present
  - Verify no HTTP traffic accepted on port 3000 externally

- [ ] **Monitoring Setup**
  ```bash
  # Watch container health
  watch -n 5 'docker-compose ps'
  
  # Monitor logs
  docker-compose logs -f
  ```

- [ ] **Backup Setup**
  ```bash
  # Create backup directory
  mkdir -p backups
  
  # Add backup cron job
  # Example: Daily backup at 2 AM
  # 0 2 * * * cd /path/to/kherashanu2 && docker-compose exec kherashanu cp /app/data/blog.db /app/data/blog.db.backup && docker cp kherashanu:/app/data/blog.db.backup ./backups/blog-$(date +\%Y\%m\%d).db
  ```

- [ ] **Auto-renewal Verification**
  ```bash
  # Verify certbot container is running
  docker-compose ps certbot
  
  # Test renewal (dry-run)
  docker-compose run --rm certbot renew --dry-run
  ```

## Production Maintenance

### Weekly
- Check container status: `docker-compose ps`
- Review logs for errors: `docker-compose logs --tail=100`

### Monthly
- Review disk usage: `df -h`
- Check certificate expiration: `docker-compose run --rm certbot certificates`
- Test backup restoration procedure

### As Needed
- Update application code and rebuild: `docker-compose up -d --build`
- Rotate logs if they grow large
- Monitor server resources: `docker stats`

## Rollback Procedure

If issues occur after deployment:

```bash
# Stop services
docker-compose down

# Restore database from backup
docker cp ./backups/blog-YYYYMMDD.db kherashanu:/app/data/blog.db

# Restart services
docker-compose up -d
```

## Support Contacts

- Let's Encrypt Status: https://letsencrypt.status.io/
- Rate Limits: https://letsencrypt.org/docs/rate-limits/
- Certificate Issues: Check logs with `docker-compose logs certbot`

# Docker Compose Quick Reference

## 🚀 Quick Start
```bash
# Build and start
docker-compose up -d --build

# View logs
docker-compose logs -f

# Stop
docker-compose down
```

## 📋 What's Included

### Services
- **kherashanu**: Main application (frontend + backend)
- **nginx**: *(commented out)* Reverse proxy for production

### Features
✅ **Data Persistence**: Database mounted to `./backend/data`  
✅ **Health Checks**: Automatic monitoring  
✅ **Environment Variables**: Configurable via `.env`  
✅ **Networking**: Isolated bridge network  
✅ **Auto-restart**: Unless manually stopped  

## 🔧 Common Commands

```bash
# View status
docker-compose ps

# Restart service
docker-compose restart kherashanu

# View logs (last 50 lines)
docker-compose logs --tail=50 kherashanu

# Execute command in container
docker-compose exec kherashanu /bin/bash

# Rebuild after code changes
docker-compose up -d --build

# Stop and remove everything (⚠️ keeps volumes)
docker-compose down

# Remove volumes too (⚠️ deletes database)
docker-compose down -v
```

## ⚠️ Note on OAuth Warning

The warning `"Cannot open Google secrets file"` is **normal** if you're using development mode with `DEV_LOGIN` code. 

To use production OAuth:
1. Create `google_oauth_secrets.json` in project root
2. Uncomment the volume mount in `docker-compose.yml`
3. Restart: `docker-compose up -d`

## 🔍 Health Check

```bash
# Check health status
docker inspect kherashanu | grep -A 10 Health

# Or via compose
docker-compose ps
```

## 📦 Production Setup

See full guide in `DOCKER.md` for:
- Nginx reverse proxy setup
- SSL/TLS configuration
- Backup strategies
- Monitoring tips

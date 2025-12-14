#!/bin/bash
set -e

# Get the absolute path to the script directory (project root)
PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "============================================"
echo "      Rebuilding and Restarting Server      "
echo "============================================"

# 1. Build Frontend
echo "[1/2] Building Frontend..."
cd "$PROJECT_ROOT/frontend"
npm run build
if [ $? -ne 0 ]; then
    echo "Error: Frontend build failed."
    exit 1
fi
echo "Frontend build complete."

# 2. Restart Backend Server
echo "[2/2] Restarting Backend Server..."
cd "$PROJECT_ROOT/backend"

# Kill existing server process
if pgrep -f "kherashanu-server" > /dev/null; then
    echo "Stopping existing server..."
    pkill -f "kherashanu-server"
    sleep 2 # Wait for port release
fi

# Start new server instance
# We run from backend dir so it finds local config/secrets if needed
# We point webroot to ../frontend/dist
echo "Starting server..."
nohup ./kherashanu-server --port 3000 --webroot ../frontend/dist > ../server.log 2>&1 &

NEW_PID=$!
echo "Server restarted successfully!"
echo "PID: $NEW_PID"
echo "Webroot: ../frontend/dist"
echo "Log: $PROJECT_ROOT/server.log"
echo "============================================"

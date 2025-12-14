# Multi-stage Dockerfile for Kherashanu Portfolio + Blog Platform

# Stage 1: Build Frontend
FROM node:20-alpine AS frontend-builder

WORKDIR /app/frontend

# Copy package files
COPY frontend/package*.json ./

# Install dependencies (using legacy-peer-deps for React 19 compatibility)
RUN npm install --legacy-peer-deps

# Copy frontend source
COPY frontend/ ./

# Build frontend
RUN npm run build

# Stage 2: Build Backend
FROM ubuntu:22.04 AS backend-builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libssl-dev \
    libcurl4-openssl-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app/backend

# Copy backend source
COPY backend/ ./

# Build backend
RUN make clean && make

# Stage 3: Final Runtime Image
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libssl3 \
    libcurl4 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Create app directory and data directory for persistence
WORKDIR /app
RUN mkdir -p /app/data

# Copy built backend binary
COPY --from=backend-builder /app/backend/kherashanu-server ./

# Copy built frontend
COPY --from=frontend-builder /app/frontend/dist ./frontend/dist

# Expose HTTP port (default 3000)
EXPOSE 3000

# Set default environment variables
ENV ADMIN_EMAIL=kherashanu@gmail.com

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:3000/ || exit 1

# Run the server
CMD ["./kherashanu-server", "--port", "3000", "--webroot", "frontend/dist", "--database", "data/blog.db"]

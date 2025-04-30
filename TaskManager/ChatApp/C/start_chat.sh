#!/bin/bash

# Error handling
set -e
trap 'cleanup' EXIT

# Cleanup function
cleanup() {
    if [ ! -z "$SERVER_PID" ]; then
        kill -15 $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
}

# Create necessary directories if they don't exist
mkdir -p downloads uploads certs

# Check if certificates exist, if not generate them
if [ ! -f certs/server.crt ] || [ ! -f certs/server.key ] || [ ! -f certs/ca.crt ]; then
    echo "Generating SSL certificates..."
    cd certs
    # Generate CA key and certificate
    openssl req -x509 -newkey rsa:4096 -keyout ca.key -out ca.crt -days 365 -nodes -subj "/CN=ChatApp CA"
    
    # Generate server key and CSR
    openssl req -newkey rsa:4096 -keyout server.key -out server.csr -nodes -subj "/CN=localhost"
    
    # Sign server certificate with CA
    openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365
    
    # Set proper permissions
    chmod 600 *.key
    chmod 644 *.crt *.csr
    cd ..
fi

echo "Starting chat server..."
./bin/chat_server 8080 &
SERVER_PID=$!

# Wait for server to start
echo "Waiting for server to initialize..."
sleep 2

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Failed to start server"
    exit 1
fi

echo "Starting chat client..."
./bin/chat_client 127.0.0.1 8080

# Cleanup will be called automatically by trap 
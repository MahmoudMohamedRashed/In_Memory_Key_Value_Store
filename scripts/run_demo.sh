#!/usr/bin/env bash
set -euo pipefail

# Path to the compiled server binary (passed as 1st argument, or defaults to ./kv_server)
SERVER_BIN="${1:-./kv_server}"
PORT=8080

# Verify that the server binary exists and is executable
if [[ ! -x "$SERVER_BIN" ]]; then
    echo "[Error] Server binary not found or not executable: $SERVER_BIN"
    echo "Usage: $0 <path_to_server_binary>"
    exit 1
fi

# 1. Launch the KV Server in a dedicated terminal window
gnome-terminal --title="KV Server" -- bash -c "\"$SERVER_BIN\"; exec bash"

# Wait briefly for the server to bind and start listening
sleep 0.5

# 2. Launch Client 1 for interactive manual testing
gnome-terminal --title="Client 1 (Interactive)" -- bash -c "echo 'Connected to KV Server on port $PORT...'; nc localhost $PORT; exec bash"

# 3. Launch Client 2 to send automated commands and display responses
gnome-terminal --title="Client 2 (Auto)" -- bash -c "
{
    echo 'PING'
    echo 'SET user1 Mahmoud'
    echo 'SET lang Modern C++20'
    echo 'GET user1'
    echo 'GET lang'
    echo 'SIZE'
} | nc -N localhost $PORT
echo '--- Automated commands finished ---'
exec bash"

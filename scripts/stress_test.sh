#!/usr/bin/env bash
set -euo pipefail

# Path to the compiled server binary (passed as 1st argument, or defaults to ./kv_server)
SERVER_BIN="${1:-./kv_server}"
PORT=8080
CLIENTS=20
OPS_PER_CLIENT=50

# Verify that the server binary exists and is executable
if [[ ! -x "$SERVER_BIN" ]]; then
    echo "[Error] Server binary not found or not executable: $SERVER_BIN"
    echo "Usage: $0 <path_to_server_binary>"
    exit 1
fi

# Start the server in the background and capture its PID
"$SERVER_BIN" &
SERVER_PID=$!

# Ensure the server shuts down gracefully with SIGINT on script exit or failure
cleanup() {
    echo ""
    echo "[Test] Stopping server (PID: $SERVER_PID)..."
    kill -SIGINT "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT

# Wait briefly for the server to initialize
sleep 0.5
echo "[Test] Launching $CLIENTS concurrent clients ($OPS_PER_CLIENT SET/GET pairs each)..."

START_TIME=$(date +%s%N)

# Spawn concurrent clients in parallel and track only their PIDs
CLIENT_PIDS=()
for c in $(seq 1 "$CLIENTS"); do
    (
        for i in $(seq 1 "$OPS_PER_CLIENT"); do
            printf "SET c%d_k%d value_%d_%d\nGET c%d_k%d\n" "$c" "$i" "$c" "$i" "$c" "$i"
        done
    ) | nc -N localhost "$PORT" > /dev/null &
    CLIENT_PIDS+=($!)
done

# Wait ONLY for the client jobs to finish (not the server)
wait "${CLIENT_PIDS[@]}"

END_TIME=$(date +%s%N)
ELAPSED_MS=$(( (END_TIME - START_TIME) / 1000000 ))

# Verify final store size to ensure thread-safety and no lost writes
EXPECTED=$(( CLIENTS * OPS_PER_CLIENT ))
ACTUAL=$(printf "SIZE\n" | nc -N localhost "$PORT" | tr -d '\r\n')

echo "[Test] Completed $(( EXPECTED * 2 )) commands in ${ELAPSED_MS} ms."
echo "[Test] Store SIZE -> Expected: $EXPECTED | Actual: $ACTUAL"

if [[ "$ACTUAL" == "$EXPECTED" ]]; then
    echo "[PASS] Multithreaded stress test passed!"
else
    echo "[FAIL] Size mismatch detected!"
    exit 1
fi

#!/usr/bin/env bash

# ApexRace Startup and Hot-Reload Script
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

export PORT="${PORT:-8080}"

# Configure CMake if not already configured
if [ ! -d "build" ]; then
    echo "[start.sh] Initializing CMake configuration..." >&2
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release >&2
fi

# Initial build of server target
echo "[start.sh] Building server target incrementally..." >&2
if ! cmake --build build --target server -j2 >&2; then
    echo "[start.sh Error] Initial build failed." >&2
    exit 1
fi

# Locate server executable
SERVER_BIN=""
if [ -f "./build/server.exe" ]; then
    SERVER_BIN="./build/server.exe"
elif [ -f "./build/server" ]; then
    SERVER_BIN="./build/server"
else
    echo "[start.sh Error] Server executable not found in build/" >&2
    exit 1
fi

SERVER_PID=""

cleanup() {
    echo "[start.sh] Shutting down ApexRace server..." >&2
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

# Start server in background
$SERVER_BIN &
SERVER_PID=$!
echo "[start.sh] Server started with PID $SERVER_PID on port $PORT." >&2

# Check if file watching tool (inotifywait, fswatch, or polling fallback) is available
echo "[start.sh] Watching source files for rebuilds..." >&2

# Compute initial hash of source/header files
get_source_fingerprint() {
    find src include -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) -exec ls -l --full-time {} + 2>/dev/null || \
    find src include -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) 2>/dev/null
}

LAST_FP=$(get_source_fingerprint)

while true; do
    sleep 2
    # Verify server is still alive
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "[start.sh Error] Server process exited unexpectedly." >&2
        wait "$SERVER_PID" || true
        exit 1
    fi

    CURRENT_FP=$(get_source_fingerprint)
    if [ "$CURRENT_FP" != "$LAST_FP" ]; then
        echo "[start.sh] Detected source modifications. Rebuilding server..." >&2
        LAST_FP="$CURRENT_FP"

        if cmake --build build --target server -j2 >&2; then
            echo "[start.sh] Rebuild succeeded! Restarting server..." >&2
            kill "$SERVER_PID" 2>/dev/null || true
            wait "$SERVER_PID" 2>/dev/null || true
            $SERVER_BIN &
            SERVER_PID=$!
            echo "[start.sh] Restarted server PID $SERVER_PID" >&2
        else
            echo "[start.sh Warning] Rebuild failed. Preserving running server." >&2
        fi
    fi
done

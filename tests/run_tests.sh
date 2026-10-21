#!/usr/bin/env bash
set -e

# Change to repository root
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

# Configure build if build directory does not exist
if [ ! -d "build" ]; then
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release >&2
fi

# Build challenge_tests incrementally, redirecting output to stderr
cmake --build build --target challenge_tests -j2 >&2

# Run test binary, preserving its exit code and stdout JSON
if [ -f "./build/challenge_tests.exe" ]; then
    ./build/challenge_tests.exe
elif [ -f "./build/challenge_tests" ]; then
    ./build/challenge_tests
else
    echo "Error: challenge_tests binary not found in build/" >&2
    exit 1
fi

#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$PROJECT_ROOT"

echo "========================================"
echo "Forge Audit"
echo "========================================"

echo
echo "[1/7] Checking code formatting..."

clang-format --dry-run --Werror \
    src/*.c \
    include/*.h \
    tests/*.c

echo "Formatting check passed."

echo
echo "[2/7] Configuring CMake..."

cmake -S . -B build

echo
echo "[3/7] Building Forge..."

cmake --build build

echo
echo "[4/7] Running protocol unit tests..."

./build/forge_protocol_test

echo
echo "[5/7] Running cppcheck..."

cppcheck \
    --enable=warning,style,performance,portability \
    --inline-suppr \
    --suppressions-list=cppcheck.suppress \
    -I include \
    src

echo
echo "[6/7] Running clang-tidy..."

for file in src/*.c; do
    echo
    echo "Analyzing: $file"

    clang-tidy \
        "$file" \
        -p build
done

echo
echo "[7/7] Audit complete."

echo
echo "========================================"
echo "Forge audit completed successfully."
echo "========================================"
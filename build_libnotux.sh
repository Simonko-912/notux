#!/bin/bash
# Build libnotux (static library) + crt0.o for user-space Notux programs.
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"

echo "Building libnotux library..."
make -C "$ROOT/libnotux" clean >/dev/null 2>&1 || true
make -C "$ROOT/libnotux" install

mkdir -p "$ROOT/build/lib"
nasm -f elf64 "$ROOT/libnotux/crt0.asm" -o "$ROOT/build/lib/crt0.o"

echo "libnotux.a + crt0.o ready in build/lib/"
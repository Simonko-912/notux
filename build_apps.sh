#!/bin/bash

# Enhanced build script for Notux OS user applications

echo "Building Notux OS applications..."

# Create bin directory if it doesn't exist
mkdir -p build/bin

# Build all applications
APPS=("hello" "init" "testapp" "calc" "filemgr" "editor" "shell" "ps" "meminfo" "nfetch" "notedit" "pkgman" "opm" "gcc_wrapper" "date" "echo" "head" "wc" "tree" "sysinfo")

for app in "${APPS[@]}"; do
    cd userspace/$app
    if [ -f Makefile ]; then
        echo "Building $app..."
        make clean 2>/dev/null || true
        make install
        if [ $? -eq 0 ]; then
            echo "✓ Built $app successfully"
        else
            echo "✗ Failed to build $app"
        fi
    else
        echo "⚠ No Makefile found for $app"
    fi
    cd ../..
done

echo ""
echo "Build complete. Applications are in build/bin/"
echo "You can now run 'make' to build the full system and test with 'make run'"

#!/usr/bin/env bash
set -e
echo ">>> Building..."
rm -rf build
meson setup build --buildtype=release
ninja -C build
echo ""
echo "✓ Done: ./build/live-wallpaper"
echo "Run:    ./build/live-wallpaper"
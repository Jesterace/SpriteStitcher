#!/usr/bin/env bash
set -euo pipefail

rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

echo
echo "Built: ./build/SpriteStitcher"
echo "Run:   ./build/SpriteStitcher"

#!/usr/bin/env bash
set -euo pipefail

rm -rf build/spritestitcher3-release

cmake -S spritestitcher3 -B build/spritestitcher3-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/spritestitcher3-release

echo
echo "Built: ./build/spritestitcher3-release/SpriteStitcher3"
echo "Run:   ./build/spritestitcher3-release/SpriteStitcher3"

#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

cmake -S spritestitcher3 -B build/spritestitcher3 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spritestitcher3

exec ./build/spritestitcher3/SpriteStitcher3

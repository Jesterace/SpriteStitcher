# SpriteStitcher 3 Prototype

This is the first experimental SpriteStitcher 3 prototype. It is intentionally separate from the SpriteStitcher v2.9.6 app in the repository root.

## Current scope

- Open a PNG sprite image.
- Display the image in a GUI window.
- Show image width and height.
- Count exact unique stitch colors.
- Treat pixels with `alpha == 0` as transparent/background pixels.
- Show each unique stitch color as a hex value in a simple table.
- Display transparent pixels over a checkerboard preview background.

Not included in this milestone: PDF export, DMC matching, Pattern Keeper export, CSV export, batch mode, website preview PNG export, or palette remapping.

## Dependencies

Install the same core Qt/C++ build tools used by SpriteStitcher v2.9.6:

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base
```

On Debian/Ubuntu/Mint:

```sh
sudo apt install build-essential cmake ninja-build qt6-base-dev
```

## Build

From the repository root:

```sh
cmake -S spritestitcher3 -B build/spritestitcher3 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spritestitcher3
```

## Test

```sh
ctest --test-dir build/spritestitcher3 --output-on-failure
```

## Run

```sh
./build/spritestitcher3/SpriteStitcher3
```

Use the `Open PNG...` button to choose a sprite image. The image preview, dimensions, unique-color count, and color table update after the file loads.

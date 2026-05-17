# SpriteStitcher 3 source tree note

Active SpriteStitcher 3 development lives in:

- spritestitcher3/
- spritestitcher3/src/

Build and run SpriteStitcher 3 with:

    ./run-spritestitcher3.sh

The old SpriteStitcher 2.9.6 source has been archived under legacy/spritestitcher-2.9.6/ and should not be edited for SpriteStitcher 3 work unless intentionally doing legacy cleanup.

# SpriteStitcher v2.9.6

SpriteStitcher is a Qt/C++ desktop app for turning sprite images into cross-stitch pattern files.

Simple workflow:

sprite PNG in -> PDF pattern / Pattern Keeper import PDF / CSV legend / website preview PNG out

## Main features

- Create cross-stitch PDF patterns from sprite images.
- Match sprite colors to nearest DMC floss colors.
- Generate Pattern Keeper-friendly import PDFs.
- Generate CSV legend / shopping list files.
- Generate transparent website preview PNGs.
- Draw red center lines on charts.
- Choose color chart, symbol chart, or both.
- Edit palette mappings before generating.
- Batch-generate multiple sprite patterns.
- Use dedicated SpriteStitcher work folders.

## Work folders

SpriteStitcher uses these folders:

- ~/Projects/SpriteStitcherWork/sprites
- ~/Projects/SpriteStitcherWork/pdfs
- ~/Projects/SpriteStitcherWork/patterns
- ~/Projects/SpriteStitcherWork/releases

The app has shortcut buttons for:

- Use work folders
- Open sprites
- Open patterns

## Build dependencies on Arch / EndeavourOS

Install the main build tools:

    sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-tools

For AppImage building:

    yay -S linuxdeploy linuxdeploy-plugin-qt

You also need appimagetool available in your PATH.

## Build from source

    cd ~/Projects/SpriteStitchCPP_main
    rm -rf build
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

Run:

    ./build/SpriteStitcher

## Build the AppImage

Use the included script:

    cd ~/Projects/SpriteStitchCPP_main
    ./scripts/build-appimage.sh

This creates:

    SpriteStitcher-v2.9.6-x86_64.AppImage

It also copies the AppImage to:

    ~/Projects/SpriteStitcherWork/releases/

## AppImage notes

The AppImage build script forces Qt to use the XCB platform plugin by default.

This avoids KDE/Wayland launch issues where Qt may ask for the Wayland platform plugin even when the AppImage does not bundle it.

The AppImage build also uses:

    NO_STRIP=1
    QMAKE=/usr/bin/qmake6

These avoid packaging problems on newer Arch-based systems.

## Project file compatibility

New project files use the SpriteStitcher project type.

Older SpriteStitchCPPProject files are still accepted so old .sstitch files continue to load.

Saved settings intentionally still use the old internal settings name for now so existing user settings are preserved.

## Version history

### v2.9.6

- Renamed app branding to SpriteStitcher.
- Updated window title to SpriteStitcher v2.9.6.
- Added work folder shortcut buttons.
- Added Linux desktop launcher.
- Added SVG app icon.
- Added repeatable AppImage build script.
- Added AppImage release workflow.
- Preserved old project/settings compatibility.

### v2.9.5

- Added website preview PNG export.
- Pattern output filenames used _v2_9_5.

### v2.9.4 and earlier

- Improved Pattern Keeper import PDFs.
- Improved symbol-key spacing.
- Added Pattern Keeper-friendly thread key pages.
- Added expanded DMC support.
- Added selectable chart symbol styles.

## SpriteStitcher 3 development build

SpriteStitcher 3 is the current development prototype.

### Build and run SpriteStitcher 3

```bash
./scripts/build-linux-v3.sh
./build/spritestitcher3-release/SpriteStitcher3

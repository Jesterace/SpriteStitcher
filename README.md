Updated for v2.9.5: Added an optional website preview PNG export generated from the final stitched pattern grid.

Updated for v2.9.4: Pattern Keeper import PDFs avoid numeric chart symbols and use roomier final Symbol Key spacing, to help cases like B5200 assigned to symbol 3.

Updated for v2.9.3: Pattern Keeper import PDFs now include a NikkiPattern-style final page with Thread lengths and Sym / No. / Colour Name symbol key.

Updated for v2.9.2: Pattern Keeper import PDF legend rows now group each symbol and DMC code into one plain text line to improve automatic color matching.

Updated for v2.9.1: the Pattern Keeper import PDF now uses the compatible-style chart layout with a chart and DMC table on the same page when it fits.

Updated for v2.9.0: generating a symbol chart now also creates a separate Pattern Keeper import PDF with ASCII symbols and a plain thread key.

Updated for v2.8.9: the Pattern Keeper Thread Key page now uses taller rows and safer spacing so entries do not overlap.

Updated for v2.8.8: PDFs now include an extra plain black-and-white Pattern Keeper Thread Key page with symbol-to-DMC rows for easier color assignment during import.

Updated for v2.8.7: choosing a new sprite image now replaces the pattern title with the new sprite filename instead of keeping the previous pattern title.

Updated for v2.8.6: added the DMC 01–35 range so the built-in DMC palette contains 489 standard floss colors.

Updated for v2.8.5: expanded the built-in DMC palette so the palette editor replacement list includes the full DMC floss list instead of only the compact matching palette.

Updated for v2.8.4: added selectable chart symbol styles (clean cross-stitch, simple icon symbols, classic SpriteStitch) used by previews, PDFs, and legends.

# SpriteStitch C++ v2.9.5 Website Preview PNG

This Linux-focused polish release is based on the stable v2.8.1/v2.8.2 app, but removes the visible batch-processing workflow to keep the interface focused on making one pattern at a time.

## What is new in v2.9.5

- Added optional website preview PNG export.
- Preview PNGs are generated from the final stitched pattern grid, so they respect background removal, DMC matching, palette overrides, and color cleanup.
- PNG previews use transparent unstitched/background squares and are useful for jesterace.com pattern cards.
- Removed the Batch queue panel from the main window.
- Simplified the left-side layout.
- Kept adjustable left-panel heights and scroll bars.
- Kept sprite preview, chart preview, zoom controls, palette editing, project save/load, and PDF layout options.
- Output PDF, CSV, Pattern Keeper PDF, and website preview PNG filenames now use `_v2_9_5`.

## EndeavourOS / Arch install

```bash
cd ~/Downloads
rm -rf SpriteStitchCPP_v2_9_5
unzip -o SpriteStitchCPP_v2_9_5.zip
cd SpriteStitchCPP_v2_9_5
./scripts/install-local.sh
```

Launch from the menu as **SpriteStitch C++**, or run:

```bash
~/.local/bin/SpriteStitchCPP
```

## Manual build

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/SpriteStitchCPP
```

## Notes

Batch-related data from older `.sstitch` project files is ignored. The rest of the project settings still load normally.

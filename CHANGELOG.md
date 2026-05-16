Updated for v2.9.5: Added optional website preview PNG export using the final stitched pattern grid for site thumbnails.

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

# Changelog

## v2.8.5 Full DMC Palette

- Removed the visible batch queue workflow from the main window.
- Simplified the left-side panel stack to pattern options, generation summary, and log.
- Kept adjustable panel heights and scroll bars.
- Updated output suffixes to `_v2_9_5`.
- Older `.sstitch` files still load; batch queue entries are ignored.

## v2.8.1 Panel Layout Polish

- Added adjustable heights for left-side panels.
- Added scrollable left-side sections where needed.
- Saved left panel sizes between launches.

## SpriteStitcher 3.0 dev1

### Added
- SpriteStitcher 3 development build scripts.
- SpriteStitcher 3 AppImage build script.
- PDF export options dialog.
- Large-pattern tiled PDF export mode.
- Practical large-pattern output for very large sprites.
- PDF QA sprite.
- Cleaner default chart symbol set.
- Improved chart symbol readability/alignment.
- Improved PDF legend sizing and page padding.
- Improved pattern summary layout.
- Crisp direct PDF chart rendering instead of raster-only chart embedding.

### Fixed
- Prevented large patterns from locking up the app with old full-page PDF export.
- Fixed symbol readability problems in PDF charts.
- Fixed awkward triangle and hollow-square symbol alignment.
- Fixed legend/table layout running too close to page edges.
- Improved large-pattern export behavior for patterns like Woodybig.

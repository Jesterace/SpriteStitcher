#!/usr/bin/env bash
set -euo pipefail

PREFIX="${HOME}/.local"

rm -f "${PREFIX}/bin/SpriteStitchCPP"
rm -f "${PREFIX}/share/applications/spritestitchcpp.desktop"
rm -f "${PREFIX}/share/icons/hicolor/scalable/apps/spritestitchcpp.svg"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${PREFIX}/share/applications" >/dev/null 2>&1 || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q "${PREFIX}/share/icons/hicolor" >/dev/null 2>&1 || true
fi

echo "Uninstalled SpriteStitch C++ from ${PREFIX}."

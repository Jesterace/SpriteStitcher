#!/usr/bin/env bash
set -euo pipefail

APP_NAME="SpriteStitcher"
DESKTOP_ID="spritestitcher.desktop"
PREFIX="${HOME}/.local"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_DIR}"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Missing cmake. On EndeavourOS: sudo pacman -S --needed cmake ninja qt6-base gcc"
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "Missing ninja. On EndeavourOS: sudo pacman -S --needed ninja"
    exit 1
fi

rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

install -Dm755 "build/${APP_NAME}" "${PREFIX}/bin/${APP_NAME}"
install -Dm644 "resources/spritestitcher.svg" "${PREFIX}/share/icons/hicolor/scalable/apps/spritestitcher.svg"
mkdir -p "${PREFIX}/share/applications"
sed \
    -e "s|^Exec=.*|Exec=${PREFIX}/bin/${APP_NAME}|" \
    -e "s|^Categories=.*|Categories=Graphics;RasterGraphics;Qt;|" \
    "resources/${DESKTOP_ID}" > "${PREFIX}/share/applications/${DESKTOP_ID}"
chmod 644 "${PREFIX}/share/applications/${DESKTOP_ID}"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${PREFIX}/share/applications" >/dev/null 2>&1 || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q "${PREFIX}/share/icons/hicolor" >/dev/null 2>&1 || true
fi

echo
echo "Installed SpriteStitcher to: ${PREFIX}/bin/${APP_NAME}"
echo "Menu launcher installed to: ${PREFIX}/share/applications/${DESKTOP_ID}"
echo "You may need to log out/in or restart your app menu if it does not appear immediately."
echo "Run from terminal with: ${PREFIX}/bin/${APP_NAME}"

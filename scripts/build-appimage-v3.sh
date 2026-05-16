#!/usr/bin/env bash
set -euo pipefail

APP_NAME="SpriteStitcher3"
VERSION="3.0.0-dev"
APPIMAGE_NAME="${APP_NAME}-v${VERSION}-x86_64.AppImage"

cd "$(dirname "$0")/.."

rm -rf build/spritestitcher3-release dist/SpriteStitcher3.AppDir "${APP_NAME}"*.AppImage

cmake -S spritestitcher3 -B build/spritestitcher3-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/spritestitcher3-release

APPDIR="$PWD/dist/SpriteStitcher3.AppDir"

mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/scalable/apps"

cp build/spritestitcher3-release/SpriteStitcher3 "$APPDIR/usr/bin/SpriteStitcher3"
cp resources/spritestitcher.svg "$APPDIR/usr/share/icons/hicolor/scalable/apps/spritestitcher3.svg"

cat > "$APPDIR/usr/share/applications/spritestitcher3.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=SpriteStitcher 3
Comment=Create cross-stitch patterns from sprite images
Exec=SpriteStitcher3
Icon=spritestitcher3
Categories=Graphics;Utility;
Terminal=false
DESKTOP

NO_STRIP=1 QMAKE=/usr/bin/qmake6 linuxdeploy \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/SpriteStitcher3" \
  --desktop-file "$APPDIR/usr/share/applications/spritestitcher3.desktop" \
  --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/spritestitcher3.svg" \
  --plugin qt

if [ -f "$APPDIR/AppRun" ]; then
  mv "$APPDIR/AppRun" "$APPDIR/AppRun.real"
fi

cat > "$APPDIR/AppRun" <<'APPRUN'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "$0")")"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$HERE/AppRun.real" "$@"
APPRUN

chmod +x "$APPDIR/AppRun" "$APPDIR/AppRun.real"

appimagetool "$APPDIR" "$APPIMAGE_NAME"
chmod +x "$APPIMAGE_NAME"

mkdir -p "$HOME/Projects/SpriteStitcherWork/releases"
cp "$APPIMAGE_NAME" "$HOME/Projects/SpriteStitcherWork/releases/"

echo
echo "Built: $PWD/$APPIMAGE_NAME"
echo "Copied to: $HOME/Projects/SpriteStitcherWork/releases/$APPIMAGE_NAME"

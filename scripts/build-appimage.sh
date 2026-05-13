#!/usr/bin/env bash
set -euo pipefail

APP_NAME="SpriteStitcher"
VERSION="$(cat VERSION | tr -d '[:space:]')"
APPIMAGE_NAME="${APP_NAME}-v${VERSION}-x86_64.AppImage"

cd "$(dirname "$0")/.."

rm -rf build dist "${APP_NAME}"*.AppImage

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build

DESTDIR="$PWD/dist/AppDir" cmake --install build

# Build AppDir and deploy Qt libraries/plugins, but do not package yet.
NO_STRIP=1 QMAKE=/usr/bin/qmake6 linuxdeploy \
  --appdir dist/AppDir \
  --desktop-file dist/AppDir/usr/share/applications/spritestitcher.desktop \
  --icon-file dist/AppDir/usr/share/icons/hicolor/scalable/apps/spritestitcher.svg \
  --plugin qt

# KDE/Wayland can request the Wayland Qt platform plugin.
# Force XCB because this AppImage is bundled/tested with the XCB platform plugin.
if [ -f dist/AppDir/AppRun ]; then
  mv dist/AppDir/AppRun dist/AppDir/AppRun.real
fi

cat > dist/AppDir/AppRun <<'APPRUN'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "$0")")"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$HERE/AppRun.real" "$@"
APPRUN

chmod +x dist/AppDir/AppRun dist/AppDir/AppRun.real

appimagetool dist/AppDir "$APPIMAGE_NAME"
chmod +x "$APPIMAGE_NAME"

mkdir -p "$HOME/Projects/SpriteStitcherWork/releases"
cp "$APPIMAGE_NAME" "$HOME/Projects/SpriteStitcherWork/releases/"

echo
echo "Built: $PWD/$APPIMAGE_NAME"
echo "Copied to: $HOME/Projects/SpriteStitcherWork/releases/$APPIMAGE_NAME"

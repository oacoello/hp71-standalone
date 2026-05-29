#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="$ROOT_DIR/linux_app"
UNPACKED_DIR="$APP_DIR/release-deb/linux-unpacked"
VERSION="1.0.1"
PACKAGE_NAME="santa-barbara-fdc"
WORK_DIR="/tmp/${PACKAGE_NAME}-deb"
PKG_DIR="$WORK_DIR/package"
OUT_DEB="$WORK_DIR/${PACKAGE_NAME}_${VERSION}_amd64.deb"
FINAL_DEB="$APP_DIR/release-deb/${PACKAGE_NAME}_${VERSION}_amd64.deb"

if [[ ! -d "$UNPACKED_DIR" ]]; then
    echo "Missing $UNPACKED_DIR. Run electron-builder first." >&2
    exit 1
fi

rm -rf "$WORK_DIR"
mkdir -p \
    "$PKG_DIR/DEBIAN" \
    "$PKG_DIR/opt/Santa Barbara FDC" \
    "$PKG_DIR/usr/bin" \
    "$PKG_DIR/usr/share/applications" \
    "$PKG_DIR/usr/share/icons/hicolor/256x256/apps"

cp -a "$UNPACKED_DIR/." "$PKG_DIR/opt/Santa Barbara FDC/"
cp -a "$APP_DIR/logo.png" "$PKG_DIR/usr/share/icons/hicolor/256x256/apps/${PACKAGE_NAME}.png"

find "$PKG_DIR" -type d -exec chmod 755 {} +
find "$PKG_DIR" -type f -exec chmod 644 {} +
chmod 755 "$PKG_DIR/opt/Santa Barbara FDC/santa-barbara-fdc"
chmod 755 "$PKG_DIR/opt/Santa Barbara FDC/chrome_crashpad_handler"
chmod 755 "$PKG_DIR/opt/Santa Barbara FDC/resources/backend/hp71_emulator"
chmod 4755 "$PKG_DIR/opt/Santa Barbara FDC/chrome-sandbox"

ln -s "/opt/Santa Barbara FDC/santa-barbara-fdc" "$PKG_DIR/usr/bin/${PACKAGE_NAME}"

cat > "$PKG_DIR/usr/share/applications/${PACKAGE_NAME}.desktop" <<'DESKTOP'
[Desktop Entry]
Name=Santa Barbara FDC
Comment=Sistema Santa Barbara FDC HP-71B
Exec=/opt/Santa Barbara FDC/santa-barbara-fdc %U
Terminal=false
Type=Application
Icon=santa-barbara-fdc
Categories=Utility;
StartupWMClass=Santa Barbara FDC
DESKTOP

INSTALLED_SIZE="$(du -sk "$PKG_DIR" | cut -f1)"

cat > "$PKG_DIR/DEBIAN/control" <<CONTROL
Package: ${PACKAGE_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Dolfo <dolfo@example.com>
Installed-Size: ${INSTALLED_SIZE}
Depends: libgtk-3-0, libnotify4, libnss3, libxss1, libxtst6, xdg-utils, libatspi2.0-0, libuuid1, libsecret-1-0
Recommends: libappindicator3-1
Homepage: https://local.santa-barbara-fdc
Description: Sistema Santa Barbara FDC HP-71B
 Aplicacion de escritorio para ejecutar el sistema Santa Barbara FDC HP-71B.
CONTROL

fakeroot dpkg-deb --build --root-owner-group "$PKG_DIR" "$OUT_DEB"
mkdir -p "$(dirname "$FINAL_DEB")"
cp -f "$OUT_DEB" "$FINAL_DEB"

echo "$FINAL_DEB"

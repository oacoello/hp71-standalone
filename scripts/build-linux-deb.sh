#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="$ROOT_DIR/linux_app"
UNPACKED_DIR="$APP_DIR/dist/linux-unpacked"
VERSION="1.0.1"
PACKAGE_NAME="calculadora-santa-barbara"
PRODUCT_NAME="Calculadora Santa Barbara"
WORK_DIR="/tmp/${PACKAGE_NAME}-deb"
PKG_DIR="$WORK_DIR/package"
OUT_DEB="$WORK_DIR/${PACKAGE_NAME}_${VERSION}_amd64.deb"
FINAL_DEB="$APP_DIR/dist/${PACKAGE_NAME}_${VERSION}_amd64.deb"

if [[ ! -d "$UNPACKED_DIR" ]]; then
    echo "Missing $UNPACKED_DIR. Run: cd linux_app && npm run pack:linux" >&2
    exit 1
fi

if [[ ! -x "$UNPACKED_DIR/resources/backend/hp71_emulator" ]]; then
    echo "Missing executable backend: $UNPACKED_DIR/resources/backend/hp71_emulator" >&2
    exit 1
fi

rm -rf "$WORK_DIR"
mkdir -p \
    "$PKG_DIR/DEBIAN" \
    "$PKG_DIR/opt/$PRODUCT_NAME" \
    "$PKG_DIR/usr/bin" \
    "$PKG_DIR/usr/share/applications" \
    "$PKG_DIR/usr/share/icons/hicolor/256x256/apps"

cp -a "$UNPACKED_DIR/." "$PKG_DIR/opt/$PRODUCT_NAME/"
cp -a "$APP_DIR/logo.png" "$PKG_DIR/usr/share/icons/hicolor/256x256/apps/${PACKAGE_NAME}.png"

find "$PKG_DIR" -type d -exec chmod 755 {} +
find "$PKG_DIR" -type f -exec chmod 644 {} +
chmod 755 "$PKG_DIR/opt/$PRODUCT_NAME/calculadora-santa-barbara"
chmod 755 "$PKG_DIR/opt/$PRODUCT_NAME/chrome_crashpad_handler"
chmod 755 "$PKG_DIR/opt/$PRODUCT_NAME/resources/backend/hp71_emulator"
chmod 4755 "$PKG_DIR/opt/$PRODUCT_NAME/chrome-sandbox" 2>/dev/null || true

ln -s "/opt/$PRODUCT_NAME/calculadora-santa-barbara" "$PKG_DIR/usr/bin/${PACKAGE_NAME}"

cat > "$PKG_DIR/usr/share/applications/${PACKAGE_NAME}.desktop" <<'DESKTOP'
[Desktop Entry]
Name=Calculadora Santa Barbara
Comment=Calculadora Santa Barbara - Sistema HP-71B
Exec=/opt/Calculadora Santa Barbara/calculadora-santa-barbara %U
Terminal=false
Type=Application
Icon=santa-barbara-fdc
Categories=Utility;
StartupWMClass=Calculadora Santa Barbara
DESKTOP

INSTALLED_SIZE="$(du -sk "$PKG_DIR" | cut -f1)"

cat > "$PKG_DIR/DEBIAN/control" <<CONTROL
Package: ${PACKAGE_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Desarrollo Mecatronica UDH - V.I.M.M
Installed-Size: ${INSTALLED_SIZE}
Depends: libgtk-3-0, libnotify4, libnss3, libxss1, libxtst6, xdg-utils, libatspi2.0-0, libuuid1, libsecret-1-0
Recommends: libappindicator3-1
Homepage: https://local.santa-barbara-fdc
Description: Calculadora Santa Barbara
 Aplicacion de escritorio para ejecutar la Calculadora Santa Barbara HP-71B.
CONTROL

mkdir -p "$(dirname "$FINAL_DEB")"

if command -v fakeroot >/dev/null 2>&1; then
    fakeroot dpkg-deb --build --root-owner-group "$PKG_DIR" "$OUT_DEB"
else
    dpkg-deb --build --root-owner-group "$PKG_DIR" "$OUT_DEB"
fi

cp -f "$OUT_DEB" "$FINAL_DEB"
echo "$FINAL_DEB"

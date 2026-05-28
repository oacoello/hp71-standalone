#!/usr/bin/env bash
set -euo pipefail

APP_NAME="santa-barbara-fdc"
PRODUCT_NAME="Santa Barbara FDC"
VERSION="1.0.1"
ARCH="amd64"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="$PROJECT_DIR/dist/linux-unpacked"
OUT="$PROJECT_DIR/dist/${APP_NAME}_${VERSION}_${ARCH}.deb"
STAGE="$(mktemp -d)"
APPDIR="$STAGE/opt/$PRODUCT_NAME"

if [[ ! -d "$SRC" ]]; then
    echo "Missing $SRC"
    echo "Run: npm run pack:linux"
    exit 1
fi

mkdir -p "$APPDIR" "$STAGE/DEBIAN" "$STAGE/usr/share/applications" "$STAGE/usr/local/bin"
cp -a "$SRC/." "$APPDIR/"

chmod 0755 "$STAGE" "$STAGE/DEBIAN"
chmod 4755 "$APPDIR/chrome-sandbox" 2>/dev/null || true
chmod +x "$APPDIR/$APP_NAME"
ln -s "/opt/$PRODUCT_NAME/$APP_NAME" "$STAGE/usr/local/bin/$APP_NAME"

cat > "$STAGE/DEBIAN/control" <<EOF
Package: $APP_NAME
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Maintainer: Dolfo <dolfo@example.com>
Depends: libgtk-3-0, libnotify4, libnss3, libxss1, libxtst6, xdg-utils, libatspi2.0-0, libuuid1, libsecret-1-0
Description: Sistema Santa Barbara FDC HP-71B
 Sistema Santa Barbara FDC HP-71B
EOF
chmod 0644 "$STAGE/DEBIAN/control"

cat > "$STAGE/usr/share/applications/$APP_NAME.desktop" <<EOF
[Desktop Entry]
Name=$PRODUCT_NAME
Exec=/opt/$PRODUCT_NAME/$APP_NAME
Terminal=false
Type=Application
Categories=Utility;
StartupWMClass=$APP_NAME
EOF
chmod 0644 "$STAGE/usr/share/applications/$APP_NAME.desktop"

dpkg-deb --build "$STAGE" "$OUT"
rm -rf "$STAGE"

echo "$OUT"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEB_PATH="${1:-$ROOT_DIR/linux_app/release-deb/santa-barbara-fdc_1.0.1_amd64.deb}"
TEST_DIR="/tmp/santa-barbara-fdc-deb-test"

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

dpkg-deb -I "$DEB_PATH" >/dev/null
dpkg-deb -x "$DEB_PATH" "$TEST_DIR"

APP_BIN="$TEST_DIR/opt/Santa Barbara FDC/santa-barbara-fdc"
BACKEND_BIN="$TEST_DIR/opt/Santa Barbara FDC/resources/backend/hp71_emulator"
INDEX_HTML="$TEST_DIR/opt/Santa Barbara FDC/resources/backend/index.html"

test -x "$APP_BIN"
test -x "$BACKEND_BIN"
test -f "$INDEX_HTML"

file "$APP_BIN"
file "$BACKEND_BIN"

BACKEND_DIR="$(dirname "$BACKEND_BIN")"
LOG_FILE="$TEST_DIR/backend.log"

(
    cd "$BACKEND_DIR"
    "$BACKEND_BIN" >"$LOG_FILE" 2>&1 &
    echo $! > "$TEST_DIR/backend.pid"
)

BACKEND_PID="$(cat "$TEST_DIR/backend.pid")"
cleanup() {
    kill "$BACKEND_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

for _ in {1..20}; do
    if curl -fsS http://127.0.0.1:8080/ 2>/dev/null | grep -q "SANTA BARBARA"; then
        echo "OK: backend served index.html"
        break
    fi
    sleep 0.25
done

curl -fsS http://127.0.0.1:8080/ | grep -q "SANTA BARBARA"
curl -fsS -X POST --data-urlencode "cmd=MAIN" http://127.0.0.1:8080/input | grep -q .
echo "OK: backend accepted input"
echo "OK: deb package structure and executable permissions verified"

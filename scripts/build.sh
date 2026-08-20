#!/bin/bash
# GBVitaEX build script — mGBA PSVita base (v2.x)
# Run from project root in devkitPro msys2 shell.
#
# Usage:
#   bash scripts/build.sh [clean|rebuild] [PSVITAIP=x.x.x.x]
#
# Builds directly from vendor/mgba using mGBA's own CMakeLists.
# Our changes are minimal surgical patches in vendor/mgba/src/platform/psp2/.

set -e

export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MGBA_SRC="$PROJECT_ROOT/vendor/mgba"
BUILD_DIR="$PROJECT_ROOT/build"

ACTION="${1:-build}"
PSVITAIP=""
for arg in "$@"; do
    case "$arg" in PSVITAIP=*) PSVITAIP="${arg#*=}" ;; esac
done

if [ "$ACTION" = "clean" ] || [ "$ACTION" = "rebuild" ]; then
    echo ">>> Cleaning build/"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ">>> Configuring (cmake against mGBA source)..."
cmake "$MGBA_SRC" \
    -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DPSP2=ON \
    -DBINARY_NAME=GBVitaEX \
    -DM_CORE_GBA=ON \
    -DM_CORE_GB=ON \
    -DBUILD_LTO=OFF \
    -DUSE_FFMPEG=OFF \
    -DUSE_SQLITE3=OFF \
    -DUSE_DISCORD_RPC=OFF \
    -DBUILD_QT=OFF \
    -DBUILD_SDL=OFF \
    -DBUILD_LIBRETRO=OFF \
    -DBUILD_HEADLESS=OFF \
    -DBUILD_PYTHON=OFF \
    -DBUILD_TEST=OFF \
    -DBUILD_SUITE=OFF \
    -DBUILD_CINEMA=OFF \
    -DBUILD_EXAMPLE=OFF \
    -DUSE_LIBZIP=OFF \
    -DENABLE_SCRIPTING=OFF \
    -DENABLE_GDB_STUB=OFF \
    -DENABLE_DEBUGGERS=OFF \
    -DUSE_ZLIB=ON \
    -DUSE_PNG=ON \
    -DUSE_ELF=OFF \
    -DUSE_LZMA=OFF \
    -G "Unix Makefiles"

echo ">>> Building..."
make -j$(nproc) GBVitaEX.vpk-vpk

echo ""
echo ">>> Done!"
VPK_PATH=$(find "$BUILD_DIR" -name "GBVitaEX.vpk" | head -1)
if [ -f "$VPK_PATH" ]; then
    ls -lh "$VPK_PATH"
    mkdir -p "$PROJECT_ROOT/release"
    cp "$VPK_PATH" "$PROJECT_ROOT/release/GBVitaEX-latest.vpk"
else
    echo "WARNING: GBVitaEX.vpk not found"
fi

if [ -n "$PSVITAIP" ] && [ -f "$VPK_PATH" ]; then
    echo ">>> Uploading to Vita at $PSVITAIP..."
    curl --ftp-method nocwd -T "$VPK_PATH" "ftp://$PSVITAIP:1337/ux0:/data/GBVitaEX.vpk"
fi

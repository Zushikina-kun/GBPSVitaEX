#!/usr/bin/env bash
# GBVitaEX build script
# Run from the project root inside the devkitPro msys2 shell:
#   bash scripts/build.sh [clean|rebuild] [PSVITAIP=x.x.x.x]
#
# Prerequisites:
#   - VitaSDK at /usr/local/vitasdk  (devkitPro msys2)
#   - cmake >= 3.12 (bundled with devkitPro msys2)

set -e

export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# Parse arguments
ACTION="${1:-build}"
EXTRA_CMAKE=""
for arg in "$@"; do
    case "$arg" in
        PSVITAIP=*) EXTRA_CMAKE="-DPSVITAIP=${arg#*=}" ;;
        DYNAREC=OFF) EXTRA_CMAKE="$EXTRA_CMAKE -DHAVE_DYNAREC=OFF" ;;
    esac
done

if [ "$ACTION" = "clean" ] || [ "$ACTION" = "rebuild" ]; then
    echo ">>> Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ">>> Configuring..."
cmake "$PROJECT_ROOT" \
    -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    $EXTRA_CMAKE \
    -G "Unix Makefiles"

echo ">>> Building..."
make -j$(nproc)

echo ""
echo ">>> Build complete!"
if [ -f GBVitaEX.vpk ]; then
    echo "    Output: $BUILD_DIR/GBVitaEX.vpk"
    ls -lh GBVitaEX.vpk
else
    echo "    WARNING: GBVitaEX.vpk not found — check build output above."
fi

# Optional deploy via FTP
if echo "$EXTRA_CMAKE" | grep -q PSVITAIP; then
    IP=$(echo "$EXTRA_CMAKE" | grep -oP '(?<=PSVITAIP=)[^ ]+')
    echo ""
    echo ">>> Uploading to Vita at $IP..."
    curl --ftp-method nocwd -T GBVitaEX.vpk "ftp://$IP:1337/ux0:/data/GBVitaEX.vpk"
fi

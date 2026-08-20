# GBVitaEX v2.x — mGBA PSVita build
#
# This project builds mGBA's PSVita port with small targeted improvements.
# See scripts/build.sh for the full build command.
#
# Quick build:
#   bash scripts/build.sh clean
#   bash scripts/build.sh PSVITAIP=x.x.x.x
#
# The actual CMakeLists is vendor/mgba/CMakeLists.txt (mGBA's own build system).
# Our changes are in vendor/mgba/src/platform/psp2/ — 4 files only.
#
# Patches applied (see git diff in vendor/mgba):
#   main.c       — sceKernelPowerTick fix (#1970), Triangle remap fix (#3039),
#                  default save/state dirs (#3039), FF mute + FPS configExtra
#   psp2-context.c — audio buffer 512→1024, BUFFERS 16→24 (#3044)
#   CMakeLists.txt — TITLEID MGBA00001→GBVX00001, PSP2 in OS_DEFINES (VFS fix),
#                    bare lib names (linker fix), --start-group (static lib fix),
#                    BUILD_LTO=OFF flag (LTO bitcode fix)
#   directories.h  — PATH_MAX guard (VitaSDK build compat)
cmake_minimum_required(VERSION 3.12)
message(FATAL_ERROR
    "Run cmake against vendor/mgba/ not the project root.\n"
    "Use: bash scripts/build.sh\n")

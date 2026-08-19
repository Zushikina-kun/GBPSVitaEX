cmake_minimum_required(VERSION 3.12)
# VitaSDK's bundled vita.cmake (and vita.toolchain.cmake) contain an old
# cmake_minimum_required(2.8) call that CMake 4.x rejects unless we set
# CMAKE_POLICY_VERSION_MINIMUM=3.5 on the command line (see build.sh).
# The variable below is a no-op on CMake < 4 and works on CMake 4+.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ─────────────────────────────────────────────────────────────────────────────
#  GBAVitaEX — Best GBA / GBC / GB emulator for PlayStation Vita
#  Engine: gpSP ARMv7 dynarec (GBA) + mGBA GB/GBC core (GB/GBC)
#  Frontend: standalone vita2d VPK, no RetroArch required
# ─────────────────────────────────────────────────────────────────────────────

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
            CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your VitaSDK installation.")
    endif()
endif()

project(GBAVitaEX C CXX ASM)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# ─────────────────────────────────────────────────────────────────────────────
#  App metadata
# ─────────────────────────────────────────────────────────────────────────────
set(VITA_APP_NAME  "GBAVitaEX")
set(VITA_TITLEID   "GBAVEX001")
set(VITA_VERSION   "01.30")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

# ─────────────────────────────────────────────────────────────────────────────
#  Options
# ─────────────────────────────────────────────────────────────────────────────
option(HAVE_DYNAREC          "Enable gpSP ARMv7 JIT dynarec"           ON)
option(ENABLE_COLOR_CORRECT  "Compile in GBA LCD colour correction"    ON)
option(ENABLE_INTERFRAME     "Compile in interframe blending"          ON)
option(ENABLE_CHEATS         "Compile cheat code engine"               ON)

# ─────────────────────────────────────────────────────────────────────────────
#  Compiler flags
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
     -O3 -ffast-math -fno-strict-aliasing -std=gnu11 \
     -march=armv7-a -mfpu=neon \
     -fomit-frame-pointer -funroll-loops")

set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -std=c++11 \
     -march=armv7-a -mfpu=neon \
     -fno-rtti -fno-exceptions")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -march=armv7-a -mfpu=neon")

# ─────────────────────────────────────────────────────────────────────────────
#  Global compile definitions
# ─────────────────────────────────────────────────────────────────────────────
add_compile_definitions(
    PSP2
    VITA
    # mGBA build flags
    M_CORE_GB
    MINIMAL_CORE=0
    ENABLE_VFS
    ENABLE_DIRECTORIES
    COLOR_5_6_5
    # gpSP dynarec flags
    CPU_ARCH=arm
    ARM_ARCH
    IOAPI_NO_64
    _GNU_SOURCE
)

if(HAVE_DYNAREC)
    # MMAP_JIT_CACHE must NOT be defined on Vita — memmap.c uses
    # #ifdef MMAP_JIT_CACHE, so defining it as 0 still triggers the mmap
    # path.  On Vita the JIT cache is allocated via sceKernelAllocMemBlock
    # directly in gba_engine.c, so we leave MMAP_JIT_CACHE undefined.
    add_compile_definitions(HAVE_DYNAREC=1)
endif()
if(ENABLE_COLOR_CORRECT)
    add_compile_definitions(ENABLE_COLOR_CORRECT)
endif()
if(ENABLE_INTERFRAME)
    add_compile_definitions(ENABLE_INTERFRAME)
endif()
if(ENABLE_CHEATS)
    add_compile_definitions(ENABLE_CHEATS)
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Include paths
# ─────────────────────────────────────────────────────────────────────────────
set(VENDOR_GPSP  "${CMAKE_SOURCE_DIR}/vendor/gpsp")
set(VENDOR_MGBA  "${CMAKE_SOURCE_DIR}/vendor/mgba")
set(LIBRETRO_COMM "${VENDOR_GPSP}/libretro/libretro-common")

include_directories(
    # Our own headers
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    # gpSP headers
    "${VENDOR_GPSP}"
    "${VENDOR_GPSP}/libretro"
    "${LIBRETRO_COMM}/include"
    # mGBA headers
    "${VENDOR_MGBA}/include"
    "${VENDOR_MGBA}/src"
    "${VENDOR_MGBA}/src/third-party/inih"
    # audio-stretch (TDHS pitch correction)
    "${CMAKE_SOURCE_DIR}/vendor/audio-stretch"
    # VitaSDK headers (toolchain usually adds these; explicit for IDE support)
    "$ENV{VITASDK}/arm-vita-eabi/include"
)

# ─────────────────────────────────────────────────────────────────────────────
#  gpSP GBA engine sources (strip libretro, keep the emulation core)
# ─────────────────────────────────────────────────────────────────────────────
set(GPSP_CORE_SOURCES
    # Main GBA hardware loop
    "${VENDOR_GPSP}/main.c"
    "${VENDOR_GPSP}/gba_memory.c"
    "${VENDOR_GPSP}/memmap.c"
    "${VENDOR_GPSP}/input.c"
    "${VENDOR_GPSP}/sound.c"
    "${VENDOR_GPSP}/savestate.c"
    "${VENDOR_GPSP}/cheats.c"
    "${VENDOR_GPSP}/serial.c"
    "${VENDOR_GPSP}/serial_proto.c"
    "${VENDOR_GPSP}/gbp.c"
    "${VENDOR_GPSP}/rfu.c"
    "${VENDOR_GPSP}/gba_cc_lut.c"
    # CPU interpreter + threaded/dynarec dispatcher (C++ files)
    "${VENDOR_GPSP}/cpu.cc"
    "${VENDOR_GPSP}/video.cc"
    # BIOS data (open-source replacement)
    "${VENDOR_GPSP}/bios_data.S"
    # libretro-common stubs we still need
    "${LIBRETRO_COMM}/compat/compat_posix_string.c"
    "${LIBRETRO_COMM}/compat/compat_strl.c"
    "${LIBRETRO_COMM}/compat/fopen_utf8.c"
    "${LIBRETRO_COMM}/encodings/encoding_utf.c"
    "${LIBRETRO_COMM}/file/file_path.c"
    "${LIBRETRO_COMM}/file/file_path_io.c"
    "${LIBRETRO_COMM}/streams/file_stream.c"
    "${LIBRETRO_COMM}/string/stdstring.c"
    "${LIBRETRO_COMM}/time/rtime.c"
    "${LIBRETRO_COMM}/vfs/vfs_implementation.c"
)

# Dynarec: cpu_threaded.c + ARM stub assembly
if(HAVE_DYNAREC)
    list(APPEND GPSP_CORE_SOURCES
        "${VENDOR_GPSP}/cpu_threaded.c"
        "${VENDOR_GPSP}/arm/arm_stub.S"
    )
    set_source_files_properties("${VENDOR_GPSP}/arm/arm_stub.S"
        PROPERTIES LANGUAGE ASM
        COMPILE_FLAGS "-march=armv7-a -mfpu=neon")
endif()

# gpSP sources require slightly relaxed warnings (old codebase)
# Also force-include retro_inline.h so INLINE is defined before any gpSP
# header (sound.h uses `static INLINE` at file scope).
# Use filename only — the gpsp root dir is already in the include path.
set(GPSP_FORCED_INCLUDE "-include retro_inline.h")

set_source_files_properties(
    "${VENDOR_GPSP}/main.c"
    "${VENDOR_GPSP}/gba_memory.c"
    "${VENDOR_GPSP}/sound.c"
    "${VENDOR_GPSP}/cpu_threaded.c"
    PROPERTIES COMPILE_FLAGS "-Wno-error -Wno-sign-compare -Wno-unused-variable ${GPSP_FORCED_INCLUDE}")
set_source_files_properties(
    "${VENDOR_GPSP}/cpu.cc"
    "${VENDOR_GPSP}/video.cc"
    PROPERTIES COMPILE_FLAGS "-Wno-error -Wno-sign-compare ${GPSP_FORCED_INCLUDE}")
# Other gpSP C files also need retro_inline.h
foreach(_f
    "${VENDOR_GPSP}/cheats.c"
    "${VENDOR_GPSP}/serial.c"
    "${VENDOR_GPSP}/serial_proto.c"
    "${VENDOR_GPSP}/gbp.c"
    "${VENDOR_GPSP}/rfu.c"
    "${VENDOR_GPSP}/gba_cc_lut.c"
    "${VENDOR_GPSP}/input.c"
    "${VENDOR_GPSP}/savestate.c"
    "${VENDOR_GPSP}/memmap.c"
    "${CMAKE_SOURCE_DIR}/src/gba/gba_engine.c"
    "${CMAKE_SOURCE_DIR}/src/core/stubs.c"
    "${CMAKE_SOURCE_DIR}/src/cheats/cheat_gba.c"
    "${CMAKE_SOURCE_DIR}/src/core/rfu_vita_net.c"
)
    set_source_files_properties(${_f}
        PROPERTIES COMPILE_FLAGS "${GPSP_FORCED_INCLUDE} -Wno-sign-compare -Wno-unused-variable")
endforeach()

# ─────────────────────────────────────────────────────────────────────────────
#  mGBA — GB/GBC core sources only (no GBA core, no Qt, no SDL)
# ─────────────────────────────────────────────────────────────────────────────
set(MGBA_GB_SOURCES
    # SM83 (Sharp LR35902) CPU
    "${VENDOR_MGBA}/src/sm83/sm83.c"
    "${VENDOR_MGBA}/src/sm83/decoder.c"
    "${VENDOR_MGBA}/src/sm83/isa-sm83.c"
    # GB/GBC board
    "${VENDOR_MGBA}/src/gb/gb.c"
    "${VENDOR_MGBA}/src/gb/core.c"
    "${VENDOR_MGBA}/src/gb/audio.c"
    "${VENDOR_MGBA}/src/gb/cheats.c"
    "${VENDOR_MGBA}/src/gb/input.c"
    "${VENDOR_MGBA}/src/gb/io.c"
    "${VENDOR_MGBA}/src/gb/mbc.c"
    "${VENDOR_MGBA}/src/gb/mbc/huc-3.c"
    "${VENDOR_MGBA}/src/gb/mbc/licensed.c"
    "${VENDOR_MGBA}/src/gb/mbc/mbc.c"
    "${VENDOR_MGBA}/src/gb/mbc/pocket-cam.c"
    "${VENDOR_MGBA}/src/gb/mbc/tama5.c"
    "${VENDOR_MGBA}/src/gb/mbc/unlicensed.c"
    "${VENDOR_MGBA}/src/gb/memory.c"
    "${VENDOR_MGBA}/src/gb/overrides.c"
    "${VENDOR_MGBA}/src/gb/serialize.c"
    "${VENDOR_MGBA}/src/gb/sio.c"
    "${VENDOR_MGBA}/src/gb/timer.c"
    "${VENDOR_MGBA}/src/gb/video.c"
    "${VENDOR_MGBA}/src/gb/renderers/software.c"
    "${VENDOR_MGBA}/src/gb/renderers/cache-set.c"
    # proxy.c is the video-logger proxy renderer — excluded (no recording needed)
    # "${VENDOR_MGBA}/src/gb/extra/proxy.c"
    "${VENDOR_MGBA}/src/gb/sio/lockstep.c"
    "${VENDOR_MGBA}/src/gb/sio/printer.c"
)

set(MGBA_CORE_SOURCES
    "${VENDOR_MGBA}/src/core/bitmap-cache.c"
    "${VENDOR_MGBA}/src/core/cache-set.c"
    "${VENDOR_MGBA}/src/core/cheats.c"
    "${VENDOR_MGBA}/src/core/config.c"
    "${VENDOR_MGBA}/src/core/core.c"
    "${VENDOR_MGBA}/src/core/directories.c"
    "${VENDOR_MGBA}/src/core/input.c"
    "${VENDOR_MGBA}/src/core/interface.c"
    "${VENDOR_MGBA}/src/core/lockstep.c"
    "${VENDOR_MGBA}/src/core/log.c"
    "${VENDOR_MGBA}/src/core/map-cache.c"
    "${VENDOR_MGBA}/src/core/mem-search.c"
    "${VENDOR_MGBA}/src/core/rewind.c"
    "${VENDOR_MGBA}/src/core/serialize.c"
    "${VENDOR_MGBA}/src/core/sync.c"
    "${VENDOR_MGBA}/src/core/timing.c"
    "${VENDOR_MGBA}/src/core/tile-cache.c"
    # Thread support (needed by lockstep.c rewind.c)
    "${VENDOR_MGBA}/src/core/thread.c"
    "${VENDOR_MGBA}/src/core/library.c"
)

set(MGBA_UTIL_SOURCES
    "${VENDOR_MGBA}/src/util/circle-buffer.c"
    "${VENDOR_MGBA}/src/util/configuration.c"
    "${VENDOR_MGBA}/src/util/crc32.c"
    "${VENDOR_MGBA}/src/util/formatting.c"
    "${VENDOR_MGBA}/src/util/gbk-table.c"
    "${VENDOR_MGBA}/src/util/hash.c"
    "${VENDOR_MGBA}/src/util/md5.c"
    "${VENDOR_MGBA}/src/util/sha1.c"
    "${VENDOR_MGBA}/src/util/string.c"
    "${VENDOR_MGBA}/src/util/table.c"
    "${VENDOR_MGBA}/src/util/vector.c"
    "${VENDOR_MGBA}/src/util/vfs.c"
    "${VENDOR_MGBA}/src/util/audio-buffer.c"
    "${VENDOR_MGBA}/src/util/audio-resampler.c"
    "${VENDOR_MGBA}/src/util/convolve.c"
    "${VENDOR_MGBA}/src/util/interpolator.c"
    "${VENDOR_MGBA}/src/util/geometry.c"
    "${VENDOR_MGBA}/src/util/image.c"
    "${VENDOR_MGBA}/src/util/patch.c"
    "${VENDOR_MGBA}/src/util/patch-fast.c"
    "${VENDOR_MGBA}/src/util/patch-ips.c"
    "${VENDOR_MGBA}/src/util/patch-ups.c"
    "${VENDOR_MGBA}/src/util/ring-fifo.c"
    "${VENDOR_MGBA}/src/util/text-codec.c"
    # inih: INI file parser used by mGBA configuration.c
    "${VENDOR_MGBA}/src/third-party/inih/ini.c"
    # PSP2 VFS + memory backends
    "${VENDOR_MGBA}/src/platform/psp2/sce-vfs.c"
    "${VENDOR_MGBA}/src/platform/psp2/psp2-memory.c"
    # VFS memory backend (used by rewind / save states)
    "${VENDOR_MGBA}/src/util/vfs/vfs-mem.c"
)

# GUI util — excluded entirely. Our UI is built on vita2d_pgf directly.
# mGBA's gui/font.c references GUIFontDrawIcon/GUIFontIconMetrics from
# gui-font.c (which requires the binary icon PNG) so the whole chain is
# unnecessary for our standalone frontend.
set(MGBA_GUI_SOURCES)

# ─────────────────────────────────────────────────────────────────────────────
#  GBAVitaEX application sources
# ─────────────────────────────────────────────────────────────────────────────
set(APP_SOURCES
    # Entry point + platform bootstrap
    "${CMAKE_SOURCE_DIR}/src/main.c"
    # Stubs: gpSP libretro globals + mGBA version strings
    "${CMAKE_SOURCE_DIR}/src/core/stubs.c"
    # Core glue: ROM detect, core dispatch, frame loop
    "${CMAKE_SOURCE_DIR}/src/core/emu_core.c"
    # Config persistence (INI save/load)
    "${CMAKE_SOURCE_DIR}/src/core/config.c"
    # GBA engine wrapper (strips libretro, drives gpSP directly)
    "${CMAKE_SOURCE_DIR}/src/gba/gba_engine.c"
    # GB/GBC engine wrapper (drives mGBA GB core)
    "${CMAKE_SOURCE_DIR}/src/gb/gb_engine.c"
    # PSP2 platform: vita2d rendering, audio thread, input
    "${CMAKE_SOURCE_DIR}/src/platform/psp2/psp2_ctx.c"
    # Audio mixing / output
    "${CMAKE_SOURCE_DIR}/src/audio/audio_output.c"
    # Save state management
    "${CMAKE_SOURCE_DIR}/src/core/savestate_mgr.c"
    # Cheat code manager
    "${CMAKE_SOURCE_DIR}/src/cheats/cheat_mgr.c"
    "${CMAKE_SOURCE_DIR}/src/cheats/cheat_gba.c"
    "${CMAKE_SOURCE_DIR}/src/cheats/cheat_gb.c"
    # GB link cable (single-device two-core lockstep)
    "${CMAKE_SOURCE_DIR}/src/gb/gb_link.c"
    # GBA RFU WiFi multiplayer (Vita UDP transport)
    "${CMAKE_SOURCE_DIR}/src/core/rfu_vita_net.c"
    # Audio time-stretch (TDHS) for fast-forward pitch correction
    # BSD licensed: https://github.com/dbry/audio-stretch
    "${CMAKE_SOURCE_DIR}/vendor/audio-stretch/stretch.c"
    # ROM browser + in-game menu UI
    "${CMAKE_SOURCE_DIR}/src/ui/ui.c"
    "${CMAKE_SOURCE_DIR}/src/ui/rom_browser.c"
    "${CMAKE_SOURCE_DIR}/src/ui/menu.c"
    "${CMAKE_SOURCE_DIR}/src/ui/settings.c"
)

# ─────────────────────────────────────────────────────────────────────────────
#  Collect icon binary objects via objcopy
# ─────────────────────────────────────────────────────────────────────────────
find_program(OBJCOPY arm-vita-eabi-objcopy)
set(OBJCOPY_FLAGS -I binary -O elf32-littlearm -B arm)

# mGBA GUI font (packed as binary object)
set(GUI_FONT_SRC "${VENDOR_MGBA}/src/platform/psp2/gui-font.c")

# ─────────────────────────────────────────────────────────────────────────────
#  mGBA sources need HAVE_LOCALE + HAVE_STRTOF_L so formatting.h doesn't
#  re-declare locale_t / strtof_l that VitaSDK's stdlib.h already provides.
# ─────────────────────────────────────────────────────────────────────────────
set(MGBA_COMPAT_DEFS HAVE_LOCALE HAVE_STRTOF_L HAVE_LOCALTIME_R)

foreach(_src ${MGBA_GB_SOURCES} ${MGBA_CORE_SOURCES} ${MGBA_UTIL_SOURCES} ${MGBA_GUI_SOURCES})
    set_source_files_properties(${_src} PROPERTIES
        COMPILE_DEFINITIONS "${MGBA_COMPAT_DEFS}")
endforeach()

# cheat_gb.c uses mGBA headers — apply the same compat defines
set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/src/cheats/cheat_gb.c"
    PROPERTIES COMPILE_DEFINITIONS "${MGBA_COMPAT_DEFS}")

# ─────────────────────────────────────────────────────────────────────────────
#  Build the executable
# ─────────────────────────────────────────────────────────────────────────────
add_executable(GBAVitaEX.elf
    ${APP_SOURCES}
    ${GPSP_CORE_SOURCES}
    ${MGBA_GB_SOURCES}
    ${MGBA_CORE_SOURCES}
    ${MGBA_UTIL_SOURCES}
    ${MGBA_GUI_SOURCES}
)

# ─────────────────────────────────────────────────────────────────────────────
#  Link libraries
# ─────────────────────────────────────────────────────────────────────────────
target_link_libraries(GBAVitaEX.elf
    # Vita display + rendering
    vita2d
    SceGxm_stub
    SceDisplay_stub
    # Audio
    SceAudio_stub
    SceAudioIn_stub
    # Input
    SceCtrl_stub
    SceTouch_stub
    SceMotion_stub
    # System
    SceKernelThreadMgr_stub
    SceKernelModulemgr_stub
    SceLibKernel_stub
    SceSysmodule_stub
    SceAppMgr_stub
    SceAppUtil_stub
    # I/O
    SceIofilemgr_stub
    # Common dialog (on-screen keyboard for renaming saves etc.)
    SceCommonDialog_stub
    # Font
    ScePgf_stub
    # Power management (clock speed control)
    ScePower_stub
    # RTC (real-time clock, used by GBA RTC games)
    SceRtc_stub
    # Misc
    SceNet_stub
    SceNetCtl_stub
    # C runtime
    m c gcc png z
)

# ─────────────────────────────────────────────────────────────────────────────
#  Compiler definitions per target (in addition to global ones)
# ─────────────────────────────────────────────────────────────────────────────
target_compile_definitions(GBAVitaEX.elf PRIVATE
    GBAVITAEX_VERSION="1.3.0"
    SAVE_PATH="ux0:data/GBAVitaEX/saves"
    STATE_PATH="ux0:data/GBAVitaEX/states"
    BIOS_PATH="ux0:data/GBAVitaEX"
    ROM_PATH="ux0:data/GBAVitaEX/roms"
    SCREENSHOT_PATH="ux0:data/GBAVitaEX/screenshots"
)

# ─────────────────────────────────────────────────────────────────────────────
#  VPK packaging
# ─────────────────────────────────────────────────────────────────────────────
vita_create_self(GBAVitaEX.self GBAVitaEX.elf UNSAFE)

vita_create_vpk(GBAVitaEX.vpk ${VITA_TITLEID} GBAVitaEX.self
    VERSION ${VITA_VERSION}
    NAME    ${VITA_APP_NAME}
    # App icons and livearea
    FILE sce_sys/icon0.png                            sce_sys/icon0.png
    FILE sce_sys/livearea/contents/bg.png             sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png        sce_sys/livearea/contents/startup.png
    FILE sce_sys/livearea/contents/template.xml       sce_sys/livearea/contents/template.xml
    # Note: Per-game overrides are compiled into gba_over.h (no separate file needed)
)

# ─────────────────────────────────────────────────────────────────────────────
#  FTP deploy helper (optional — set PSVITAIP in cmake cache or env)
# ─────────────────────────────────────────────────────────────────────────────
if(NOT DEFINED PSVITAIP)
    set(PSVITAIP "192.168.1.100" CACHE STRING "PS Vita IP for FTP deploy")
endif()

add_custom_target(send
    COMMAND curl --ftp-method nocwd -T GBAVitaEX.vpk
            ftp://${PSVITAIP}:1337/ux0:/data/GBAVitaEX.vpk
    DEPENDS GBAVitaEX.vpk
    COMMENT "Uploading GBAVitaEX.vpk to Vita at ${PSVITAIP}"
)

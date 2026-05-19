cmake_minimum_required(VERSION 3.16)
project(tonio VERSION 1.0 LANGUAGES C)

include(GNUInstallDirs)

# Media backend option: opus (default) or vlc
set(MEDIA_BACKEND "opus" CACHE STRING "Media backend: opus or vlc")

# Validate backend option
if(NOT MEDIA_BACKEND STREQUAL "opus" AND NOT MEDIA_BACKEND STREQUAL "vlc")
    message(FATAL_ERROR "Unknown media backend '${MEDIA_BACKEND}'. "
                        "Supported backends: opus, vlc")
endif()

# ── Required packages via pkg-config ──────────────────────────────────────────
find_package(PkgConfig REQUIRED)

pkg_check_modules(MHD         REQUIRED libmicrohttpd>=0.9.72)
pkg_check_modules(ALSA        REQUIRED alsa)
pkg_check_modules(CONFUSE     REQUIRED libconfuse)
pkg_check_modules(GPIOD       REQUIRED libgpiod>=2)

# Wireless tools (iwlib.h) – no pkg-config, check header + find library
include(CheckIncludeFile)
check_include_file(iwlib.h HAVE_IWLIB_H)
if(NOT HAVE_IWLIB_H)
    message(FATAL_ERROR "Cannot find required header iwlib.h (wireless_tools)")
endif()
find_library(IW_LIBRARY iw)
if(NOT IW_LIBRARY)
    message(FATAL_ERROR "Cannot find libiw (wireless_tools)")
endif()

# ── Media-backend-specific dependencies ───────────────────────────────────────
if(MEDIA_BACKEND STREQUAL "vlc")
    pkg_check_modules(VLC REQUIRED libvlc)
    set(MEDIA_SRC media_vlc.c)
    set(MEDIA_BACKEND_VLC 1)
else()
    pkg_check_modules(OPUSFILE REQUIRED opusfile)
    set(MEDIA_SRC media_opus.c)
    set(MEDIA_BACKEND_OPUS 1)
endif()

# Optional: check library for tests
pkg_check_modules(CHECK check)

# ── Generate config.h ─────────────────────────────────────────────────────────
configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)

# ── Subdirectories ────────────────────────────────────────────────────────────
add_subdirectory(src)
add_subdirectory(www)

if(CHECK_FOUND)
    add_subdirectory(tests)
endif()

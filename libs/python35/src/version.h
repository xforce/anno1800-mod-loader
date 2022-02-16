#pragma once

// clang-format off
#define VERSION_MAJOR               0
#define VERSION_MINOR               8
#define VERSION_REVISION            5

#define STRINGIFY_(s)               #s
#define STRINGIFY(s)                STRINGIFY_(s)

#define VER_FILE_DESCRIPTION_STR    "Anno 1800 Mod Loader"
#define VER_FILE_VERSION            VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION,0
#define VER_FILE_VERSION_STR        STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_REVISION)

#define VER_PRODUCTNAME_STR         "Anno 1800 Mod Loader"
#define VER_PRODUCT_VERSION          VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION
#define VER_PRODUCT_VERSION_STR     VER_FILE_VERSION_STR
#define VER_ORIGINAL_FILENAME_STR   "anno1800-mod-loader.dll"
#define VER_INTERNAL_NAME_STR       VER_ORIGINAL_FILENAME_STR
#define VER_COPYRIGHT_STR           "Copyright (C) 2020"

#ifdef DEBUG
#define VER_FILEFLAGS               VS_FF_DEBUG
#else
#define VER_FILEFLAGS               0
#endif

#ifndef VS_VERSION_INFO
#define VS_VERSION_INFO 1
#endif
// clang-format on

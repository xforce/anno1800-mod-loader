cc_import(
    name = "python_lib",
    interface_library = "Python35/libs/python35.lib",
    system_provided = True,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "python",
    hdrs = glob(["Python35/include/**/*.h"]),
    includes = [
        "Python35/include",
    ],
    visibility = ["//visibility:public"],
    deps = [":python_lib"],
)

cc_library(
    name = "ksignals",
    hdrs = ["ksignals/include/ksignals/ksignals.h"],
    includes = [
        "ksignals/include",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "libudis86",
    srcs = glob(["libudis86/src/**/*.h"]) + glob(["libudis86/src/**/*.c"]),
    hdrs = glob(["libudis86/include/**/*.h"]),
    includes = [
        "libudis86/include",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "json",
    hdrs = glob(["json/single_include/json.hpp"]),
    includes = [
        "json/single_include",
    ],
    visibility = ["//visibility:public"],
)


cc_library(
    name = "spdlog",
    defines = ["SPDLOG_WCHAR_FILENAMES"] + select({
        "@bazel_tools//src/conditions:windows": ["SPDLOG_WCHAR_TO_UTF8_SUPPORT"],
        "//conditions:default": [],
    }),
    hdrs = glob(["spdlog/include/**/*.h"]),
    includes = [
        "spdlog/include",
    ],
    visibility = ["//visibility:public"],
)

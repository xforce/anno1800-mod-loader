config_setting(
    name = "internal",
    values = {"define": "internal=1"},
)

load("@io_bazel//src/main/res:win_res.bzl", "windows_resources")

windows_resources(
    name = "python_res",
    rc_files = ["src/python.rc"],
    resources = ["src/version.h"],
)

cc_binary(
    name = "python35.dll",
    srcs = glob(["src/**/*.cc"]) + glob(["src/**/*.h"]) + glob(["include/**/*.h"]),
    defines = ["WIN32_LEAN_AND_MEAN"] + select({
        ":internal": [
            "INTERNAL_ENABLED",
        ],
        "//conditions:default": [],
    }),
    linkopts = [
        "-DEFAULTLIB:user32.lib",
        "-DEFAULTLIB:Shell32.lib",
    ],
    linkshared = 1,
    win_def_file = "src/python_forward.def",
    deps = [
        ":python_res",
        ":loader_interface",
        "//third_party:json",
        "//libs/external-file-loader:external-file-loader",
        "//libs/python-binding:python-binding",
        "//third_party:python",
        "//third_party:ksignals",
        "//third_party:spdlog",
        "@meow_hook//:meow-hook",
    ] + select({
        ":internal": [
            "//libs/internal/debuggable:debuggable",
        ],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "loader_interface",
    hdrs = ["include/interface.h"],
    includes = [
        "include/",
    ],
    visibility = ["//visibility:public"],
)

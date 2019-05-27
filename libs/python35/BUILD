config_setting(
    name = "internal",
    values = {"define": "internal=1"},
)

cc_binary(
    name = "python35.dll",
    srcs = glob(["src/**/*.cc"]) + glob(["src/**/*.h"]) + glob(["include/**/*.h"]),
    defines = ["_WINSOCKAPI_"] + select({
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
        ":loader_interface",
        "//third_party:json",
        "//libs/hooking:hooking",
        "//libs/external-file-loader:external-file-loader",
        "//third_party:python",
        "//third_party:ksignals",
        "//third_party:spdlog",
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

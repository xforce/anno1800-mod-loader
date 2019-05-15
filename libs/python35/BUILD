config_setting(
    name = "internal",
    values = {"define": "internal=1"},
)

cc_binary(
    name = "python35.dll",
    srcs = glob(["src/**/*.cc"]) + glob(["src/**/*.h"]) + glob(["include/**/*.h"]),
    defines = select({
        ":internal": [
            "INTERNAL_ENABLED",
        ],
        "//conditions:default": [],
    }),
    linkshared = 1,
    win_def_file = "src/python_forward.def",
    deps = [
        ":loader_interface",
        "//libs/hooking:hooking",
        "//third_party:python",
        "//third_party:ksignals",
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

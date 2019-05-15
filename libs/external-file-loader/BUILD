cc_library(
    name = "external-file-loader",
    srcs = glob([
        "src/**/*.cc",
    ]) + glob([
        "src/**/*.h",
    ]),
    hdrs = ["include/external-file-loader.h"],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "//libs/anno-api",
        "//libs/hooking",
        "//libs/python35:loader_interface",
        "//third_party:ksignals",
    ],
)

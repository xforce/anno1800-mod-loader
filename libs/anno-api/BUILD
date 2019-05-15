cc_library(
    name = "anno-api",
    srcs = glob([
        "src/**/*.cc",
    ]) + glob([
        "src/**/*.h",
    ]),
    hdrs = glob([
        "include/**/*.h",
    ]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "//libs/hooking",
    ],
)

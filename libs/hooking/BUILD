cc_library(
    name = "hooking",
    srcs = glob([
        "include/**/*.h",
    ]) + glob([
        "src/**/*.h",
        "src/**/*.cc",
    ]),
    hdrs = glob([
        "include/**/*.h",
    ]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party:ksignals",
        "//third_party:libudis86",
    ],
)

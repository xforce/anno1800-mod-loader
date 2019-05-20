cc_library(
    name = "xml-operations",
    srcs = glob([
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
        "@com_google_absl//absl/strings",
        "@libxml",
    ],
)

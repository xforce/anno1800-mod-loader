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
        "//third_party:spdlog",
        "@com_google_absl//absl/strings",
        "@pugixml",
    ],
)

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
        "//libs/xml-operations",
        "//libs/python35:loader_interface",
        "//third_party:ksignals",
        "//third_party:spdlog",
        "//third_party:json",
        "@boringssl//:crypto",
        "@com_google_absl//absl/strings",
        "@com_github_facebook_zstd//:libzstd",
        "@libxml",
    ],
)

cc_library(
    name = "python-binding",
    srcs = glob([
        "src/**/*.cc",
    ]) + glob([
        "src/**/*.h",
    ]),
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "//libs/anno-api",
        "//libs/python35:loader_interface",
        "//third_party:json",
        "//third_party:ksignals",
        "//third_party:spdlog",
        "@com_google_absl//absl/strings",
        "@meow_hook//:meow-hook",
    ],
)

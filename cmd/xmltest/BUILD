package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "xmltest",
    srcs = glob(["src/**/*.cc"]) + glob(["src/**/*.h"]) + glob(["include/**/*.h"]),
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": [
            "-lstdc++fs",
            "-ldl",
        ],
    }),
    deps = [
        "//libs/xml-operations",
    ],
)

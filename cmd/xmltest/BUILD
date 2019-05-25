cc_binary(
    name = "xmltest",
    srcs = glob(["src/**/*.cc"]) + glob(["src/**/*.h"]) + glob(["include/**/*.h"]),
    deps = [
        "//libs/xml-operations",
    ],
    linkopts = select({
            "@bazel_tools//src/conditions:windows": [],
            "//conditions:default": ["-lstdc++fs"],
    }),
)

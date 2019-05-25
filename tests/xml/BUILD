package(default_visibility = ["//visibility:private"])

load("@//tools/skylib:run_binary.bzl", "run_binary")
load("@//tools/skylib:write_file.bzl", "write_file")

write_file(
    name = "meow",
    out = "meow.bat",
    content =
        select({
            "@bazel_tools//src/conditions:windows": ["python %1 %2"],
            "//conditions:default": ["python $1 $2"],
        }),
    is_executable = True,
)

run_binary(
    name = "gen_tests",
    srcs = glob(
        [
            "**/*.xml",
            "**/*.json",
        ],
    ) + [
        "gen_tests.py",
    ],
    outs = [
        "tests2.inc.hpp",
    ],
    args = [
        "$(location gen_tests.py)",
        "$(location tests2.inc.hpp)",
    ],
    tool = ":meow",
)

cc_test(
    name = "xml-tests",
    srcs = [
        "main.cc",
        ":gen_tests",
    ],
    data = glob([
        "**/*.xml",
        "**/*.json",
    ]),
    includes = ["."],
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": [
            "-lstdc++fs",
            "-ldl",
        ],
    }),
    deps = [
        "//libs/xml-operations",
        "//third_party:Catch2",
        "//third_party:json",
    ],
)

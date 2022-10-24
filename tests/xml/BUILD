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
        "runner.h",
    ],
    outs = [
        "tests.cc",
    ],
    args = [
        "$(location gen_tests.py)",
        "$(location tests.cc)",
    ],
    tool = ":meow",
)

filegroup(
    name = "test-files",
    srcs = glob([
        "**/*.xml",
        "**/*.json",
    ]),
)

cc_test(
    name = "xml-tests",
    srcs = [
        "main.cc",
        "runner.h",
        ":gen_tests",
    ],
    data = [
        ":test-files"
    ],
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
        "@catch2//:catch2",
        "//third_party:json",
    ],
)

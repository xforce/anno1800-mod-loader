load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# bazel-skylb 0.8.0 released 2019.03.20 (https://github.com/bazelbuild/bazel-skylib/releases/tag/0.8.0)
skylib_version = "1.3.0"

git_repository(
    name = "io_bazel",
    commit = "15371720ae0c40ffc97b74c871d1b38851ef6410",
    shallow_since = "1638468745 +0100",
    remote = "https://github.com/bazelbuild/bazel.git",
)

http_archive(
    name = "catch2",
    strip_prefix = "Catch2-2.13.10",
    url = "https://github.com/catchorg/Catch2/archive/v2.13.10.tar.gz",
)

http_archive(
    name = "bazel_skylib",
    type = "tar.gz",
    url = "https://github.com/bazelbuild/bazel-skylib/releases/download/{}/bazel-skylib-{}.tar.gz".format(skylib_version, skylib_version),
    sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
)

# abseil-cpp
http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-8c0b94e793a66495e0b1f34a5eb26bd7dc672db0",
    urls = ["https://github.com/abseil/abseil-cpp/archive/8c0b94e793a66495e0b1f34a5eb26bd7dc672db0.zip"],  # Abseil LTS branch, June 2022, Patch 1
    sha256 = "b9f490fae1c0d89a19073a081c3c588452461e5586e4ae31bc50a8f36339135e",
)

http_archive(
    name = "com_github_facebook_zstd",
    build_file = "@//:zstd.BUILD",
    strip_prefix = "zstd-e47e674cd09583ff0503f0f6defd6d23d8b718d3",
    urls = [
        "https://github.com/facebook/zstd/archive/e47e674cd09583ff0503f0f6defd6d23d8b718d3.tar.gz",
    ],
    sha256 = "2a7b7e5d3f8c759894f0fea9917a590733600574d20cb53f3be827c7c62862e1"
)

new_local_repository(
    name = "pugixml",
    build_file = "pugixml.BUILD",
    path = "./third_party/pugixml",
)

http_archive(
    name = "boringssl",
    strip_prefix = "boringssl-863837047be74b20f59369b5256c39ad00f015b3",
    sha256 = "55c19c2db2e8b23fc3d19ad707209f992da5b3b6cfbe3812e6aadcee1786669c",
    urls = [
        "https://github.com/google/boringssl/archive/863837047be74b20f59369b5256c39ad00f015b3.zip",
    ],
)

local_repository(
    name = "meow_hook",
    path = "third_party/meow-hook",
)

load("@io_bazel//src/main/res:winsdk_configure.bzl", "winsdk_configure")
winsdk_configure(name = "local_config_winsdk")
load("@local_config_winsdk//:toolchains.bzl", "register_local_rc_exe_toolchains")
register_local_rc_exe_toolchains()
register_toolchains("@io_bazel//src/main/res:empty_rc_toolchain")

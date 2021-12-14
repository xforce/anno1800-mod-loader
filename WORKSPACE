load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# bazel-skylb 0.8.0 released 2019.03.20 (https://github.com/bazelbuild/bazel-skylib/releases/tag/0.8.0)
skylib_version = "1.1.1"

git_repository(
    name = "io_bazel",
    tag = "4.2.2",
    remote = "https://github.com/bazelbuild/bazel.git",
)

http_archive(
    name = "bazel_skylib",
    type = "tar.gz",
    url = "https://github.com/bazelbuild/bazel-skylib/releases/download/{}/bazel-skylib.{}.tar.gz".format(skylib_version, skylib_version),
)

# abseil-cpp
http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-215105818dfde3174fe799600bb0f3cae233d0bf",
    urls = ["https://github.com/abseil/abseil-cpp/archive/215105818dfde3174fe799600bb0f3cae233d0bf.zip"],  # Abseil LTS branch, Feb 2020, Patch 2
)

http_archive(
    name = "com_github_facebook_zstd",
    build_file = "@//:zstd.BUILD",
    strip_prefix = "zstd-a488ba114ec17ea1054b9057c26a046fc122b3b6",
    urls = [
        "https://github.com/facebook/zstd/archive/a488ba114ec17ea1054b9057c26a046fc122b3b6.tar.gz",
    ],
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

load("@//tools/res:winsdk_configure.bzl", "winsdk_configure")

winsdk_configure(name = "local_config_winsdk")

load("@local_config_winsdk//:toolchains.bzl", "register_local_rc_exe_toolchains")

register_local_rc_exe_toolchains()

register_toolchains("@//tools/res:empty_rc_toolchain")

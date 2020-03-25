load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# bazel-skylb 0.8.0 released 2019.03.20 (https://github.com/bazelbuild/bazel-skylib/releases/tag/0.8.0)
skylib_version = "0.8.0"

http_archive(
    name = "bazel_skylib",
    sha256 = "2ef429f5d7ce7111263289644d233707dba35e39696377ebab8b0bc701f7818e",
    type = "tar.gz",
    url = "https://github.com/bazelbuild/bazel-skylib/releases/download/{}/bazel-skylib.{}.tar.gz".format(skylib_version, skylib_version),
)

# abseil-cpp
http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-2f76a9bf50046e396138cc8eeb3cdc17b7a5ac24",
    urls = ["https://github.com/abseil/abseil-cpp/archive/2f76a9bf50046e396138cc8eeb3cdc17b7a5ac24.zip"],  # 2019-03-14
)

http_archive(
    name = "com_github_facebook_zstd",
    build_file = "@//:zstd.BUILD",
    sha256 = "1333b186377b6e86607160b4f5659c86c266b6fb9e7e8b52c7947c3611fc8b5b",
    strip_prefix = "zstd-83b51e9f886be7c2a4d477b6e7bc6db831791d8d",
    urls = [
        "https://github.com/facebook/zstd/archive/83b51e9f886be7c2a4d477b6e7bc6db831791d8d.tar.gz",
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
    urls = [
        "https://github.com/google/boringssl/archive/863837047be74b20f59369b5256c39ad00f015b3.zip",
    ],
)

local_repository(
    name = "meow_hook",
    path = "third_party/meow-hook",
)

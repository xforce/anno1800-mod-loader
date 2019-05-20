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
    sha256 = "71d00d15fe6370220b6685552fb66e5814f4dd2e130f3836fc084c894943753f",
    strip_prefix = "abseil-cpp-7c7754fb3ed9ffb57d35fe8658f3ba4d73a31e72",
    urls = ["https://github.com/abseil/abseil-cpp/archive/7c7754fb3ed9ffb57d35fe8658f3ba4d73a31e72.zip"],  # 2019-03-14
)

new_local_repository(
    name = "libxml",
    build_file = "libxml.BUILD",
    path = "./third_party/libxml2",
)

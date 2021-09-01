package(default_visibility = ["//visibility:public"])

cc_library(
    name = "libzstd",
    deps = [
        ":common",
        ":compress",
        ":decompress",
        ":deprecated",
        ":zstd_header",
    ],
)

cc_library(
    name = "zdict",
    hdrs = [
        "lib/dictBuilder/zdict.h",
        "lib/dictBuilder/divsufsort.h",
        "lib/dictBuilder/cover.h",
    ],
    strip_include_prefix = "lib/dictBuilder",
    srcs = glob(["lib/dictBuilder/*.c"]),
    deps = [":common"],
)

cc_library(
    name = "debug",
    srcs = ["lib/common/debug.c", "lib/common/debug.h"],
    hdrs = ["lib/common/debug.h"],
    strip_include_prefix = "lib/common",
)

cc_library(
    name = "bitstream",
    hdrs = ["lib/common/bitstream.h"],
    strip_include_prefix = "lib/common",
)

cc_library(
    name = "compiler",
    hdrs = ["lib/common/compiler.h"],
    strip_include_prefix = "lib/common",
)

cc_library(
    name = "cpu",
    hdrs = ["lib/common/cpu.h"],
)

cc_library(
    name = "errors",
    deps = [
        ":errors_public",
    ],
    hdrs = [
        "lib/common/error_private.h",
        "lib/common/zstd_deps.h"
    ],
    srcs = ["lib/common/error_private.c",  "lib/common/error_private.h"],
    strip_include_prefix = "lib/common",
)

cc_library(
    name = "errors_public",
    hdrs = [
        "lib/zstd_errors.h",
    ],
    srcs = ["lib/zstd_errors.h"],
    strip_include_prefix = "lib",
)

cc_library(
    name = "mem",
    hdrs = [
        "lib/common/mem.h",
    ],
    strip_include_prefix = "lib/common",
)

cc_library(
    name = "legacy",
    hdrs = glob(["lib/legacy/*.h"]),
    srcs = glob(["lib/legacy/*.c"]),
    deps = [":common"],
    copts = ["-DZSTD_LEGACY_SUPPORT=4"],
)

cc_library(
    name = "decompress",
    hdrs = glob([("lib/decompress/*_impl.h")]),
    srcs = glob(["lib/decompress/zstd*.c"]) + [
        "lib/decompress/zstd_decompress_block.h",
        "lib/decompress/zstd_decompress_internal.h",
        "lib/decompress/zstd_ddict.h",
    ],
    strip_include_prefix = "lib/decompress",
    deps = [
        ":common",
        ":legacy",
    ],
)

cc_library(
    name = "deprecated",
    hdrs = glob(["lib/deprecated/*.h"]),
    srcs = glob(["lib/deprecated/*.c"]),
    deps = [":common"],
)

cc_library(
    name = "compress",
    hdrs = [
        "lib/compress/zstd_compress_literals.h",
        "lib/compress/zstd_compress_internal.h",
        "lib/compress/zstd_compress_sequences.h",
        "lib/compress/zstd_compress_superblock.h",
        "lib/compress/zstd_cwksp.h",
        "lib/compress/zstd_double_fast.h",
        "lib/compress/zstd_fast.h",
        "lib/compress/zstd_lazy.h",
        "lib/compress/zstd_ldm.h",
        "lib/compress/zstd_ldm_geartab.h",
        "lib/compress/zstdmt_compress.h",
        "lib/compress/zstd_opt.h",
    ],
    srcs = [
        "lib/compress/zstd_compress.c",
        "lib/compress/zstd_compress_literals.c",
        "lib/compress/zstd_compress_sequences.c",
        "lib/compress/zstd_compress_superblock.c",
        "lib/compress/zstd_double_fast.c",
        "lib/compress/zstd_fast.c",
        "lib/compress/zstd_lazy.c",
        "lib/compress/zstd_ldm.c",
        "lib/compress/zstdmt_compress.c",
        "lib/compress/zstd_opt.c",
        "lib/compress/hist.c",
    ],
    deps = [":common"],
)

cc_library(
    name = "hist",
    hdrs = ["lib/compress/hist.h"],
    strip_include_prefix = "lib/compress",
)

cc_library(
    name = "threading",
    deps = [
        ":debug"
    ],
    hdrs = ["lib/common/threading.h"],
    srcs = ["lib/common/threading.c"],
    linkopts = ["-pthread"],
    copts = ["-DZSTD_MULTITHREAD"],
)

cc_library(
    name = "pool",
    hdrs = ["lib/common/pool.h"],
    srcs = ["lib/common/pool.c"],
    deps = [
        ":debug",
        ":threading",
        ":zstd_common",
    ],
)

cc_library(
    name = "xxhash",
    hdrs = [
        "lib/common/xxhash.h",
        "lib/common/zstd_deps.h",
    ],
    deps = [
        ":debug",
        ":compiler",
        ":mem"
    ],
    srcs = ["lib/common/xxhash.c"],
    copts = [
        "-DXXH_NAMESPACE=ZSTD_",
    ],
)

cc_library(
    name = "zstd_header",
    hdrs = ["lib/zstd.h", "lib/zstd_errors.h"],
    strip_include_prefix = "lib",
)

cc_library(
    name = "zstd_common",
    hdrs = [
        "lib/common/zstd_internal.h",
    ],
    srcs = ["lib/common/zstd_common.c", "lib/common/xxhash.c", "lib/common/xxhash.h"],
    deps = [
        ":compiler",
        ":debug",
        ":entropy",
        ":errors",
        ":mem",
        ":zstd_header",
    ],
)

cc_library(
    name = "entropy",
    hdrs = [
        "lib/common/fse.h",
        "lib/common/huf.h",
        "lib/compress/hist.h",
    ],
    srcs = [
        "lib/compress/hist.h",
        "lib/common/entropy_common.c",
        "lib/common/fse_decompress.c",
        "lib/compress/fse_compress.c",
        "lib/compress/huf_compress.c",
        "lib/decompress/huf_decompress.c",
    ],
    includes = ["lib/common"],
    deps = [
        ":debug",
        ":bitstream",
        ":compiler",
        ":errors",
        ":hist",
        ":mem",
        ":threading",
        ":xxhash",
    ],
)

cc_library(
    name = "common",
    deps = [
        ":debug",
        ":bitstream",
        ":compiler",
        ":cpu",
        ":entropy",
        ":errors",
        ":mem",
        ":pool",
        ":threading",
        ":xxhash",
        ":zstd_common",
    ],
)

cc_library(
    name = "util",
    hdrs = ["programs/util.h"],
    deps = [":mem", ":platform"],
)

cc_library(
    name = "datagen",
    hdrs = ["programs/datagen.h"],
    srcs = ["programs/datagen.c"],
    deps = [":mem", ":platform"],
)

cc_library(
    name = "platform",
    hdrs = ["programs/platform.h"],
)

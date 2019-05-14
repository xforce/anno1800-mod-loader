cc_import(
    name = "python_lib",
    interface_library = "Python35/libs/python35.lib",
    system_provided = True,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "python",
    hdrs = glob(["Python35/include/**/*.h"]),
    includes = [
        "Python35/include",
    ],
    visibility = ["//visibility:public"],
    deps = [":python_lib"],
)

cc_library(
    name = "ksignals",
    hdrs = ["ksignals/include/ksignals/ksignals.h"],
    includes = [
        "ksignals/include",
    ],
    visibility = ["//visibility:public"],
)

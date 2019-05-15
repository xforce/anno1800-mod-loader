load("@//tools/skylib:run_binary.bzl", "run_binary")
load("@//tools/skylib:write_file.bzl", "write_file")

write_file(
    name = "script",
    out = "script.bat",
    content = [
        "cd third_party\\libxml2\\win32",
        "cscript configure.js compiler=msvc iconv=no zlib=no debug=no http=no ftp=no",
        "cd ../../../",
        "powershell -command \"cp third_party/libxml2/include/libxml/xmlversion.h %1\"",
        "powershell -command \"cp third_party/libxml2/config.h %2\""
    ],
    is_executable = True,
)


run_binary(
    name = "libxml_configure",
    tool = ":script",
    srcs = [
        "include/libxml/xmlversion.h.in"
    ],
    args = [
        "$(location include/libxml/xmlversion.h)",
        "$(location config.h)"
    ],
    outs = [
        "include/libxml/xmlversion.h",
        "config.h",
    ]
)

cc_library(
    name = "libxml",
    srcs = [
        "HTMLparser.c",
        "HTMLtree.c",
        "SAX.c",
        "SAX2.c",
        "buf.c",
        "c14n.c",
        "catalog.c",
        "chvalid.c",
        "debugXML.c",
        "dict.c",
        "encoding.c",
        "entities.c",
        "error.c",
        "globals.c",
        "hash.c",
        "legacy.c",
        "list.c",
        "nanoftp.c",
        "nanohttp.c",
        "parser.c",
        "parserInternals.c",
        "pattern.c",
        "relaxng.c",
        "schematron.c",
        "threads.c",
        "tree.c",
        "uri.c",
        "valid.c",
        "xinclude.c",
        "xlink.c",
        "xmlIO.c",
        "xmlmemory.c",
        "xmlmodule.c",
        "xmlreader.c",
        "xmlregexp.c",
        "xmlsave.c",
        "xmlschemas.c",
        "xmlschemastypes.c",
        "xmlstring.c",
        "xmlunicode.c",
        "xmlwriter.c",
        "xpath.c",
        "xpointer.c",
        "xzlib.c",
    ],
    hdrs = glob(
        [
            "*.h",
            "include/libxml/*.h",
        ],
        exclude = [
            "config.h",
            "include/libxml/xmlversion.h",
        ],
    ) + [
        "config.h",
        "include/libxml/xmlversion.h",
    ],
    copts = [
        "-D_REENTRANT",
        "-DHAVE_CONFIG_H",
        "-w",
    ],
    includes = [
        ".",
        "include",
    ],
    visibility = ["//visibility:public"],
)
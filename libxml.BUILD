load("@//tools/skylib:run_binary.bzl", "run_binary")
load("@//tools/skylib:write_file.bzl", "write_file")

write_file(
    name = "script",
    out = "script.bat",
    content = select({
        "@bazel_tools//src/conditions:windows": [
            "cd third_party\\libxml2\\win32",
            "cscript configure.js compiler=msvc iconv=no zlib=no debug=no http=no ftp=no",
            "cd ../../../",
            "powershell -command \"cp third_party/libxml2/include/libxml/xmlversion.h %2\"",
            "powershell -command \"cp third_party/libxml2/config.h %3\"",
        ],
        "//conditions:default": [
            "export PATH=$PATH:$4",
            "meow=$(pwd)",
            "cd $(dirname $1)",
            "autoreconf -if -Wall 2&> /dev/null",
            "./configure --without-http --without-zlib --without-lzma --without-iconv --without-ftp",
            "mkdir -p $(dirname $meow/$2); cp include/libxml/xmlversion.h $meow/$2",
            "mkdir -p $(dirname $meow/$3); cp config.h $meow/$3",
        ],
    }),
    is_executable = True,
)

run_binary(
    name = "libxml_configure",
    srcs = glob(
        [
            "**/*.*",
            "**/*",
        ],
        exclude = [
            "config.h",
            "include/libxml/xmlversion.h",
        ],
    ),
    outs = [
        "config.h",
        "include/libxml/xmlversion.h",
    ],
    args = [
        "$(location autogen.sh)",
        "$(location include/libxml/xmlversion.h)",
        "$(location config.h)",
        "$PATH",
        "2",
    ],
    tool = ":script",
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
        ":libxml_configure",
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

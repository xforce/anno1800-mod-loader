
#define MODLOADER_VERSION "10"

#include "./parseArguments.h"

#include <memory>
#include <string>

using namespace std;

static void printUsage()
{
    printf("xmltest using modloader %s\n", MODLOADER_VERSION);

    printf("\nUsage: xmltest.exe [options] target patch\n\n");
    printf("-m=<path>  Specify mod path. Default: working directory\n");
    printf("           Multiple are allowed.\n");
    printf("-i         Read patch content from stdin. Path path is still required.\n");
    printf("-o         Output file. Default: patched.xml\n");
    printf("-s         Skip output.\n");
    printf("-v         Verbose.\n");
}

static bool invalidUsage(const std::string& pArg)
{
    fprintf(stderr, "Invalid argument: %s\n", pArg.c_str());
    printUsage();
    return false;
}

bool parseArguments(int argc, const char* argv[], XmltestParameters& params)
{
    if (argc < 3) {
        printUsage();
        return false;
    }

    params.skipOutput = false;
    params.useStdin = false;
    params.verbose = false;
    for (int i = 0; i < argc; i++) {
		if (argv[i] == std::string("-v")) {
			params.verbose = true;
        }
    }

    if (params.verbose) {
        printf("xmltest using modloader %s\n", MODLOADER_VERSION);
    }

    char lastMode = 0;
    for (int i = 1; i < argc; i++) {
        std::string pArg = argv[i];
        if (pArg[0] == '-') {
            if (pArg.length() <= 1 || lastMode != 0) {
                return invalidUsage(pArg);
            }
            switch (pArg[1]) {
                case 'v': {
                    break;
                }
                case 'i': {
                    if (params.useStdin) {
                        return invalidUsage(pArg);
                    }
                    params.useStdin = true;
                    lastMode = 'i';
                    break;
                }
                case 's': {
                    if (params.skipOutput || !params.outputFile.empty()) {
                        return invalidUsage(pArg);
                    }
                    params.skipOutput = true;
                    break;
                }
                case 'o': {
                    if (params.skipOutput || !params.outputFile.empty()) {
                        return invalidUsage(pArg);
                    }
                    lastMode = 'o';
                    break;
                }
                case 'p':
                case 'm': {
                    lastMode = pArg[1];
                    break;
                }
                default: {
                    return invalidUsage(pArg);
                }
            }
        }
        else if (lastMode != 0) {
            switch (lastMode) {
                case 'o': {
                    params.outputFile = pArg;
                    break;
                }
                case 'm': {
                    params.modPaths.emplace_back(pArg);
                    break;
                }
                case 'p': {
                    params.prepatchPaths.emplace_back(pArg);
                    break;
                }
                case 'i': {
                    params.stdinPath = pArg;
                    break;
                }
                default: {
                    return invalidUsage(pArg);
                }
            }
            lastMode = 0;
        }
        else {
            if (params.targetPath.empty()) {
                params.targetPath = pArg;
            }
            else if (params.patchPath.empty()) {
                params.patchPath = pArg;
            }
            else {
                return invalidUsage(pArg);
            }
        }
    }

    if (lastMode != 0) {
        printUsage();
        return false;
    }

    if (params.patchPath.empty()) {
        fprintf(stderr, "Specify target and patch file.\n");
        printUsage();
        return false;
    }

    if (params.modPaths.empty()) {
        params.modPaths.emplace_back(std::filesystem::current_path());
    }

    return true;
}

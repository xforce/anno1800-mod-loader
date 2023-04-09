
#pragma once

#include <filesystem>
#include <vector>

struct XmltestParameters {
	bool verbose;
	bool skipOutput;
	std::filesystem::path targetPath;
	std::filesystem::path patchPath;
	std::filesystem::path outputFile;
	std::vector<std::filesystem::path> modPaths;
	std::vector<std::filesystem::path> prepatchPaths;

	bool useStdin;
	std::filesystem::path stdinPath;
};

bool parseArguments(int argc, const char* argv[], XmltestParameters& params);

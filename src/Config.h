#pragma once

// Include
#include <optional>
#include <string>

// Default config
struct Config
{
    int width               = 1920;
    int height              = 1080;
    std::string output_file = "";
    std::string output_dir  = "";
    std::optional<int> seed;
    float frequency         = 0.003f;
    int octaves             = 5;
    int numRegions          = 6;
};

// -h menu
void printUsage(const char* programName);

// System date [for filenames]
std::string getSystemDate();

// Creates dir if missing
void ensureDirectory(const std::string& dir);

// Parse args and flags before generator
Config parseArgs(int argc, char* argv[]);

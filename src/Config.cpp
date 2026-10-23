#include "Config.h"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>

// Print help message
void printUsage(const char* programName)
{
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -w, --width <int>       Map width  (default: 1920)\n"
              << "  -h, --height <int>      Map height (default: 1080)\n"
              << "  -m, --merc-map          Generate a Mercator Map (5632x2160) | OVERRIDES -w && -h\n"
              << "  -s, --seed <int>        Noise seed (default: random)\n"
              << "  -f, --frequency <float> Base noise frequency (default: 0.0003)\n"
              << "      --octaves <int>     Fractal Octaves (default: 5)\n"
              << "      --regions <int>     Number of regions (default: 6)\n"
              << "  -o, --output <string>   Output PNG filename (default: current date)\n"
              << "  -d, --output-dir <path> Output directory (default: /filename/)\n"
              << "      --help              Show this message\n";
}

std::string getSystemDate()
{
    std::time_t time = std::time({});
    // Format of the string
    char timeString[std::size("yyyy-mm-ddThh:mm:ssZ")];
    // Get data in specified format
    std::strftime(std::data(timeString), std::size(timeString), "%FT%TZ", std::localtime(&time));

    return timeString;
}

void ensureDirectory(const std::string& dir)
{
    std::filesystem::create_directory(dir); // Doesn't do anything if the folder already exists
}

Config parseArgs(int argc, char* argv[])
{
    Config config;

    for (int i = 1; i < argc; i++)
    {
        // Get argument
        std::string arg = argv[i];

        // Get value after flag
        auto nextValue = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc)
            {
                // No value specified
                throw std::runtime_error("Missing value for " + flag);
            }
            return argv[++i];
        };

        // Assign specified value to relative config parameter
        if (arg == "-w" || arg == "--width")
        {
            config.width = std::stoi(nextValue(arg));
        }
        else if (arg == "-h" || arg == "--height")
        {
            config.height = std::stoi(nextValue(arg));
        }
        else if (arg == "-m" || arg == "--merc-map")
        {
            config.width  = 5632;
            config.height = 2160;
        }
        else if (arg == "-o" || arg == "--output")
        {
            config.output_file = nextValue(arg);
        }
        else if (arg == "-d" || arg == "--output-dir" || arg == "--output_dir")
        {
            config.output_dir = nextValue(arg);
        }
        else if (arg == "-s" || arg == "--seed")
        {
            config.seed = std::stoi(nextValue(arg));
        }
        else if (arg == "-f" || arg == "--frequency")
        {
            config.frequency = std::stof(nextValue(arg));
        }
        else if (arg == "--octaves")
        {
            config.octaves = std::stoi(nextValue(arg));
        }
        else if (arg == "--regions")
        {
            config.numRegions = std::stoi(nextValue(arg));
        }
        else if (arg == "--help")
        {
            printUsage(argv[0]);
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    // Sets defaults
    if (config.width  <= 0) config.width  = 1920;
    if (config.height <= 0) config.height = 1080;

    if (config.output_file.empty())
    {
        config.output_file = getSystemDate();
    }

    if (config.output_dir.empty())
    {
        config.output_dir = getSystemDate();
    }

    // If no seed was specified, randomly generate one
    if (!config.seed.has_value())
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1, 1000000);
        config.seed = dist(gen);
        std::cout << "No seed specified. Using generated seed" << std::endl;
    }

    std::cout << "Seed: " << config.seed.value() << std::endl;
    // Append seed to filename
    config.output_file += "_S_";
    config.output_file += std::to_string(config.seed.value());

    return config;
}

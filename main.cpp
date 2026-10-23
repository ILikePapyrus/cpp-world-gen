// Include
// internal
#include <exception>
#include <iostream>
#include <string>

// custom
#include "src/Config.h"
#include "src/ScopedTimer.h"
#include "src/TerrainGenerator.h"

int main(int argc, char* argv[])
{
    try
    {
        // Edit the default values with the arguments passed from cmd
        Config config = parseArgs(argc, argv);
        ensureDirectory(config.output_dir);

        // Create generator obj
        TerrainGenerator generator(config);

        {
            ScopedTimer timer("World Generation");
            generator.generateWorld();
        }

        {
            ScopedTimer timer("Border Generation");
            generator.generateRegions();
        }

        // Create paths for each img that will be saved to disk
        std::string terrainPath   = config.output_dir + "/" + config.output_file + ".png";
        std::string regionPath    = config.output_dir + "/" + config.output_file + "_REGION.png";
        std::string heightmapPath = config.output_dir + "/" + config.output_file + "_HEIGHTMAP.png";

        {
            ScopedTimer timer("Save to disk");
            generator.saveToImage(terrainPath);
            generator.saveRegionMap(regionPath);
            generator.saveHeightMap(heightmapPath);
        }

        std::cout << "Filename: " << config.output_file << std::endl;
        std::cout << "Saved to: " << config.output_dir  << "/" << std::endl;
    } catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
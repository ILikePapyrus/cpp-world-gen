#pragma once

// Include
#include <string>
#include <vector>

#include "Config.h"

class TerrainGenerator
{
public:
    // Data relative to the world that will be saved
    struct WorldData
    {
        std::vector<float> elevation;   // Terrain
        std::vector<float> moisture;    // Biomes
        std::vector<float> temperature; // Biomes
        std::vector<int>   regionId;    // Borders
    };

    explicit TerrainGenerator(const Config& cfg);

    // Generation steps
    void generateWorld();   // Elevation, moisture, temperature
    void generateRegions(); // regionId, needs a world [generateWorld()]

    // Export
    void saveToImage(const std::string& filename) const;   // Biome map + border overlay
    void saveRegionMap(const std::string& filename) const; // Flat region colors
    void saveHeightMap(const std::string& filename) const; // Grayscale elevation

    // Getters
    const Config&             getConfig()      const { return config; }
    int                       getWidth()       const { return config.width; }
    int                       getHeight()      const { return config.height; }
    const WorldData&          getWorld()       const { return world; }
    const std::vector<float>& getElevation()   const { return world.elevation; }
    const std::vector<float>& getMoisture()    const { return world.moisture; }
    const std::vector<float>& getTemperature() const { return world.temperature; }

private:
    Config    config;
    WorldData world;
};

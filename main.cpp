#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>
#include <iterator>
#include <ctime>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <random>
#include <thread>
#include <filesystem>

#include "FastNoiseLite.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

class ScopedTimer {
    std::chrono::high_resolution_clock::time_point start;
    std::string label;
public:
    ScopedTimer(const std::string& label) : label(label) {
        start = std::chrono::high_resolution_clock::now();
    }
    ~ScopedTimer() {
        std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
        std::cout << label << ": " << elapsed.count() << " seconds" << std::endl;
    }
};

struct Config {
    int width               = 1920;
    int height              = 1080;
    std::string output_file = "";
    std::string output_dir  = "";
    std::optional<int> seed;
    float frequency         = 0.003f;
    int octaves             = 5;
    int numRegions          = 6;
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -w, --width <int>       Map width  (default: 1920)\n"
              << "  -h, --height <int>      Map height (default: 1080)\n"
              << "  -s, --seed <int>        Noise seed (default: random)\n"
              << "  -f, --frequency <float> Base noise frequency (default: 0.0003)\n"
              << "      --octaves <int>     Fractal Octaves (default: 5)\n"
              << "      --regions <int>     Number of regions (default: 6)\n"
              << "  -o, --output <path>     Output PNG filename (default: date)\n"
              << "  -d, --output-dir <path> Output directory (default: /filename/)\n"
              << "      --help              Show this message\n";  
}

std::string getSystemDate() {
    std::time_t time = std::time({});
    // Format of the string that will be used
    char timeString[std::size("yyyy-mm-ddThh:mm:ssZ")];
    // Gets data in the format requested
    std::strftime(std::data(timeString), std::size(timeString), "%FT%TZ", std::localtime(&time));
            
    return timeString;
}

void ensureDirectory(const std::string& dir) {
    std::filesystem::create_directory(dir); // Doesn't do anything if the folder already exists
}

Config parseArgs(int argc, char* argv[]) {
    Config config;
           
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        // Get value after the flag
        auto nextValue = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                // There's no value specified
                throw std::runtime_error("Missing value for " + flag);
            }
            return argv[++i];
        };
        
        // Assign specified value to relative config parameter
        if (arg == "-w" || arg == "--width") {
            config.width = std::stoi(nextValue(arg));
        } 
        else if (arg == "-h" || arg == "--height") {
            config.height = std::stoi(nextValue(arg));
        }
        else if (arg == "-o" || arg == "--output") {
            config.output_file = nextValue(arg);
        }
        else if (arg == "-d" || arg == "--output_dir") {
            config.output_dir = nextValue(arg);
        }
        else if (arg == "-s" || arg == "--seed") {
            config.seed = std::stoi(nextValue(arg));
        }
        else if (arg == "-f" || arg == "--frequency") {
            config.frequency = std::stof(nextValue(arg));
        }
        else if (arg == "--octaves") {
            config.octaves = std::stoi(nextValue(arg));
        }
        else if (arg == "--regions") {
            config.numRegions = std::stoi(nextValue(arg));
        }
        else if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    
    // Sets defaults
    if (config.width  <= 0) config.width  = 1920;
    if (config.height <= 0) config.height = 1080;
           
    if (config.output_file.empty()) {
        config.output_file = getSystemDate();
    }

    if (config.output_dir.empty()) {
        config.output_dir = getSystemDate();
    }
    
    // If no seed was specified via cmd flags, randomly chooses one
    if (!config.seed.has_value()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1, 1000000);
        config.seed = dist(gen);
        std::cout << "No seed specified. using generated seed" << std::endl;
    }
    
    std::cout << "Seed: " << config.seed.value() << std::endl;
    // Appends seed to the filename
    config.output_file += "_S_";
    config.output_file += std::to_string(config.seed.value());
        
    return config;
}

class terrainGenerator {
    private:
        // Data relative to the world that will be saved
        struct WorldData {
            std::vector<float> elevation;    // Terrain
            std::vector<float> moisture;     // Biomes
            std::vector<float> temperature;  // Biomes
            std::vector<int>   regionId;     // Borders
        };
        struct ContinentCenter { float x, y, radiusX, radiusY, rotation, strength; }; // Generate more convincing continents
        struct RegionCenter { float x, y; };                                          // Used for border generation
        struct Color { uint8_t r, g, b; };                                            // Used to color the biomes
          
        WorldData world;        
        Config config;
                              
        std::vector<int> labelLandmasses(const std::vector<float>& elevation, int width, int height, int& landmassCount) {
            std::vector<int> landId(width * height, -1);
            int currentId = 0;
            
            auto isLand = [&](int idx) {
                float e = (elevation[idx] + 1.0f) * 0.5f;
                return e >= 0.50f;
            };
            
            std::vector<int> stack;
            for (int start = 0; start < width * height; start++) {
                if (!isLand(start) || landId[start] != -1) continue;
                
                // BFS/DFS flood fill
                landId[start] = currentId;
                stack.push_back(start);
                
                while (!stack.empty()) {
                    int idx = stack.back();
                    stack.pop_back();
                    int x = idx % width;
                    int y = idx / width;
                    
                    static const int dx[] = {1, -1, 0, 0};
                    static const int dy[] = {0, 0, 1, -1};
                    for (int d = 0; d < 4; d++) {
                        int nx = x + dx[d], ny = y + dy[d];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                        int nIdx = ny * width + nx;
                        if (isLand(nIdx) && landId[nIdx] == -1) {
                            landId[nIdx] = currentId;
                            stack.push_back(nIdx);
                        }
                    }
                }
                currentId++;
            }
            
            landmassCount = currentId;
            return landId;
        }
        
        std::vector<RegionCenter> generateRegionSeeds(const std::vector<float>& elevation, int width, int height, int seed, int totalRegions) {
            int landmassCount = 0;
            std::vector<int> landId = labelLandmasses(elevation, width, height, landmassCount);
            
            // Group land pixel indices by landmass
            std::vector<std::vector<int>> pixelsByLandmass(landmassCount);
            for (int idx = 0; idx < width * height; idx++) {
                if (landId[idx] >= 0) {
                    pixelsByLandmass[landId[idx]].push_back(idx);
                }
            }
            
            // Total land area
            size_t totalLandPixels = 0;
            for (auto& px : pixelsByLandmass) totalLandPixels += px.size();
            
            std::mt19937 rng (seed + 777);
            std::vector<RegionCenter> seeds;
            
            for (int i = 0; i < landmassCount; i++) {
                if (pixelsByLandmass[i].empty()) continue;
                
                // Proportional share of totalRegions, min 1 * landmass
                int allocated = std::max(1, (int)std::round(
                    totalRegions * (double)pixelsByLandmass[i].size() / (double)totalLandPixels
                ));
                
                std::shuffle(pixelsByLandmass[i].begin(), pixelsByLandmass[i].end(), rng);
                int toPlace = std::min(allocated, (int)pixelsByLandmass[i].size());
                
                for (int j = 0; j < toPlace; j++) {
                    int idx = pixelsByLandmass[i][j];
                    seeds.push_back({(float)(idx % width), (float)(idx / width)});
                }
            }
            
            return seeds;
        }
        
        std::vector<int> assignRegions(const std::vector<RegionCenter>& seeds, int width, int height, int warpSeed) {
            std::vector<int> regionId(width * height, -1);
            
            // Warp noise used to distort the straight borders generated by Voronoi graph
            FastNoiseLite borderWarp;
            borderWarp.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
            borderWarp.SetSeed(warpSeed + 999);
            borderWarp.SetFrequency(0.003f);
            borderWarp.SetFractalType(FastNoiseLite::FractalType_DomainWarpProgressive);
            borderWarp.SetFractalOctaves(2);
            borderWarp.SetDomainWarpAmp(width * 0.015f);
            
            auto computeRows = [&](int yStart, int yEnd) {
                for (int y = yStart; y < yEnd; y++) {
                    int index = y * width;
                    for (int x = 0; x < width; x++) {
                        float wx = (float)x;
                        float wy = (float)y;
                        borderWarp.DomainWarp(wx, wy);
                        
                        float bestDist = std::numeric_limits<float>::max();
                        int bestId = -1;
                        for (size_t i = 0; i < seeds.size(); i++) {
                            float dx = wx - seeds[i].x;
                            float dy = wy - seeds[i].y;
                            float  d = dx * dx + dy * dy;
                            if (d < bestDist) {
                                bestDist = d;
                                bestId = (int)i;
                            }
                        }
                        
                        regionId[index++] = bestId;
                    }
                }
            };
            
            unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
            std::vector<std::thread> threads;
            int rowsPerThread = height/numThreads;
            for (unsigned int i = 0; i < numThreads; i++) {
                int yStart = i * rowsPerThread;
                int yEnd   = (i == numThreads - 1) ? height : yStart + rowsPerThread;
                threads.emplace_back(computeRows, yStart, yEnd);
            }
            for (auto& t : threads) t.join();
            
            return regionId;
        }
        
        std::vector<ContinentCenter> generateContinentCenters(int width, int height, int seed, int count) {
            std::mt19937 rng(seed);
            std::uniform_real_distribution<float> xDist(width * 0.08f, width * 0.92f);
            std::uniform_real_distribution<float> yDist(height * 0.10f, height * 0.90f);
            std::uniform_real_distribution<float> radiusDist(width * 0.10f, width * 0.22f);
            std::uniform_real_distribution<float> stretchDist(0.5f, 1.0f);
            std::uniform_real_distribution<float> rotDist(0.0f, 3.14159f);
            std::uniform_real_distribution<float> strengthDist(0.7f, 1.0f);            
            
            std::vector<ContinentCenter> centers;
            for (int i = 0; i < count; i++) {
                float baseRadius = radiusDist(rng);
                centers.push_back({
                    xDist(rng), yDist(rng),
                    baseRadius,                    // radiusX
                    baseRadius * stretchDist(rng), // radiusY (shorter than radiusX to avoid circles)
                    rotDist(rng),
                    strengthDist(rng)
                });
            }
            return centers;
        }
        
        float continentBump(float x, float y, const std::vector<ContinentCenter>& centers) {
            float total = 0.0f;
            
            for (const auto& c : centers) {
                float dx = x - c.x;
                float dy = y - c.y;
                
                // Rotate in ellipse's local space
                float cosA = std::cos(c.rotation), sinA = std::sin(c.rotation);
                float localX = dx * cosA + dy * sinA;
                float localY = -dx * sinA + dy * cosA;
                
                float dist = std::sqrt((localX * localX) / (c.radiusX * c.radiusX) + (localY * localY) / (c.radiusY * c.radiusY));
                float bump = std::max(0.0f, 1.0f - dist);
                
                bump = bump * bump * (3.0f - 2.0f * bump); // Smoothstep taper
                total += bump * c.strength;
            }
            
            return std::min(total, 1.0f);
        }
        
        // Choose the biome color based on WorldGen parameters
        Color getBiomeColor(float elevation, float moisture, float temperature) {
            float e = (elevation + 1.0f) * 0.5f; // Normalize [0, 1]
            
            // Water and high-elevation
            if (e < 0.40f) return { 20,  40, 110}; // Deep water
            if (e < 0.46f) return { 60, 110, 200}; // Shallow water
            if (e < 0.50f) return {210, 200, 150}; // Sand
            if (e > 0.85f) return {255, 255, 255}; // Snow
            if (e > 0.75f) return {130, 120, 110}; // Rock/Mountain
            
            // Land biomes
            if (temperature > 0.65f) {
                if (moisture < 0.25f) return {230, 210, 130}; // Desert
                if (moisture < 0.55f) return {200, 185, 100}; // Savanna
                return {60, 140, 40};                         // Tropical rainforest
            } else if (temperature > 0.35f) {
                if (moisture < 0.25f) return {180, 175, 120}; // Dry shrubland/steppe
                if (moisture < 0.60f) return { 90, 150,  60}; // Grassland
                return {40, 110, 50};                         // Temperate forest
            } else {
                if (moisture < 0.30f) return {190, 195, 190}; // Tundra
                return {70, 110, 90};                         // Taiga/boreal forest
            }
        }
        
        // Calculate distance between pixel and border to make islands and not terrain that will be cutoff by the image borders
        float calculateFalloff(int x, int y, int width, int height) {
            // Normalize coords [-1, 1]
            float nx = (2.0f * x / width)  - 1.0f;
            float ny = (2.0f * y / height) - 1.0f;
            
            float xFalloff = std::max(0.0f, std::abs(nx) - 0.85f) / 0.15f; // Fades left/right edges
            float yFalloff = std::max(0.0f, std::abs(ny) - 0.85f) / 0.15f; // Past 85% (top/bottom)
            yFalloff = std::min(yFalloff, 1.0f);
            
            // Distance from center
            float distance = std::max(xFalloff, yFalloff);
            
            // Clamp distance [0, 1]
            distance = std::min(distance, 1.0f);
            
            // Cubic fallout curve
            // Used to make sure the coastline isn't a small circle in the middle
            return distance * distance * distance;
        }
                
    public:   
        terrainGenerator(const Config& cfg) : config(cfg) {
            // Start WorldData generation timer
            ScopedTimer timer("Generation");
            // Generate WorldData
            world = generate(config.width, config.height, config.seed.value(), config.frequency, config.octaves);
            
            // Generate and assign regions based on WorldData
            auto seeds = generateRegionSeeds(world.elevation, config.width, config.height, config.seed.value(), config.numRegions);
            world.regionId = assignRegions(seeds, config.width, config.height, config.seed.value());
            
            // Print total number of pixels generated
            std::cout << "TOT: " << world.elevation.size() << std::endl;
        }
        
        // Actually generate the world
        WorldData generate(int width, int height, int seed, float frequency, int octaves) {
            // Generate the elevation
            FastNoiseLite elevNoise;
            elevNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            elevNoise.SetSeed(seed);
            elevNoise.SetFrequency(frequency);
            elevNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
            elevNoise.SetFractalOctaves(octaves);
            elevNoise.SetFractalLacunarity(2.0f);
            elevNoise.SetFractalGain(0.5f);            
            
            // Add details to the elevation
            FastNoiseLite detail;
            detail.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            detail.SetSeed(seed + 1); // Offset so it's not identical to elevation'
            detail.SetFrequency(0.013f);
            detail.SetFractalType(FastNoiseLite::FractalType_FBm);
            detail.SetFractalOctaves(3);
            
            // Generate the moisture map
            FastNoiseLite moistureNoise;
            moistureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            moistureNoise.SetFrequency(0.0015f);
            moistureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
            moistureNoise.SetFractalOctaves(4);
            moistureNoise.SetSeed(1337);
            
            // Generate the temperature map
            FastNoiseLite tempNoise;
            tempNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            tempNoise.SetFrequency(0.002f);
            tempNoise.SetSeed(9001);

            // Generate the warp noise to make the world seem more natural
            FastNoiseLite warpNoise;
            warpNoise.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
            warpNoise.SetFrequency(0.0015f);
            warpNoise.SetFractalType(FastNoiseLite::FractalType_DomainWarpProgressive);
            warpNoise.SetFractalOctaves(3);
            warpNoise.SetDomainWarpAmp(width * 0.06f); // Scales with map size
            
            // Actually save the noises
            WorldData world;
            world.elevation.resize(width * height);
            world.moisture.resize(width * height);
            world.temperature.resize(width * height);
            
            // Generate the centers of the landmasses
            auto majorcenters = generateContinentCenters(width, height, seed, 4);
            auto islandCenters = generateContinentCenters(width, height, seed + 500, 8);
            
            // Apply the noisemaps to each other
            auto computeRows = [&](int yStart, int yEnd) {
                for (int y = yStart; y < yEnd; y++) {
                    int index = y * width;
                    for (int x = 0; x < width; x++) {
                        float wx = (float)x;
                        float wy = (float)y;
                        warpNoise.DomainWarp(wx, wy);
                        
                        // Elevation                       
                        float continentMask = std::max(continentBump(wx, wy, majorcenters), continentBump(wx, wy, islandCenters) * 0.6f);
                        float baseNoise = elevNoise.GetNoise(wx, wy);
                        float detailNoise = detail.GetNoise(wx, wy);
                        
                        float e = (continentMask * 2.0f - 1.0f) * 0.75f + baseNoise * 0.5f + detailNoise * 0.08f;
                        e -= calculateFalloff(x, y, width, height) * 2.0f;       // Pull edges toward deep water
                        e  = std::max(-1.0f, std::min(1.0f, e));             // Keep the value in range
                    
                        // Moisture
                        float m = moistureNoise.GetNoise((float)x, (float)y);
                        m = (m + 1.0f) * 0.5f; // Normalize [0, 1]
                    
                        // Temperature
                        float latitude = std::abs((y / (float)height) - 0.5f) * 2.0f; // 0 == equator | 1 == pole
                        float t = 1.0f - latitude;                                       // Hot at equator
                        t += tempNoise.GetNoise((float)x, (float)y) * 0.15f;       // Local variation
                        t  = std::max(0.0f, std::min(1.0f, t));
                    
                        world.elevation[index]   = e;
                        world.moisture[index]    = m;
                        world.temperature[index] = t;
                        index++;
                    }
                }
            };
            
            // Multithread the generation process
            unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
            std::vector<std::thread> threads;
            int rowsPerThread = height / numThreads;
            
            for (unsigned int i = 0; i < numThreads; i++) {
                int yStart = i * rowsPerThread;
                int yEnd   = (i == numThreads - 1) ? height : yStart + rowsPerThread;
                threads.emplace_back(computeRows, yStart, yEnd);
            }
            for (auto& thread : threads) {
                thread.join();
            }
                        
            return world;
        }
        
        // Save the biome map to png
        void saveToImage(const std::string filename) {
            std::vector<uint8_t> imageData(config.width * config.height * 3);
            
            for (size_t i = 0; i < world.elevation.size(); i++) {
                Color c = getBiomeColor(world.elevation[i], world.moisture[i], world.temperature[i]);
                
                imageData[i * 3 + 0] = c.r;
                imageData[i * 3 + 1] = c.g;
                imageData[i * 3 + 2] = c.b;
            }
            
            const Color borderColor = {255, 0, 0};
            int width = config.width, height = config.height;
            
            // Draw Voronoi borders on the biome map
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int idx = y * width + x;
                    float e = (world.elevation[idx] + 1.0f) * 0.5f;
                    if (e < 0.46f) continue; // Skip oceans
                    
                    int myRegion = world.regionId[idx];
                    int checkRadius = 2;
                    bool isBorder = (x + checkRadius < width && world.regionId[idx + checkRadius] != myRegion) ||
                                    (y + checkRadius < height && world.regionId[idx + checkRadius * width] != myRegion);
                                    
                    if (isBorder) {
                        imageData[idx * 3 + 0] = borderColor.r;
                        imageData[idx * 3 + 1] = borderColor.g;
                        imageData[idx * 3 + 2] = borderColor.b;
                    }
                }
            }
            
            // Write to disk
            int result = stbi_write_png(filename.c_str(), config.width, config.height, 3, imageData.data(), config.width * 3);
            
            if (result) {
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR SAVING THE IMAGE" << std::endl;
            }
        }
        
        // Save the Voronoi region map to disk
        void saveRegionMap(const std::string filename) {
            // Generate random colors for each region
            std::mt19937 colorRng(12345);
            std::uniform_int_distribution<int> colorDist(30, 220);
            
            int maxId = *std::max_element(world.regionId.begin(), world.regionId.end());
            std::vector<Color> regionColors(maxId + 1);
            for (auto& c : regionColors) {
                c = {(uint8_t)colorDist(colorRng), (uint8_t)colorDist(colorRng), (uint8_t)colorDist(colorRng)};
            }
            
            std::vector<uint8_t> imageData(config.width * config.height * 3);
            for (size_t i = 0; i < world.regionId.size(); i++) {
                Color c = (world.regionId[i] >= 0) ? regionColors[world.regionId[i]] : Color{0, 0, 0};
                imageData[i * 3 + 0] = c.r;
                imageData[i * 3 + 1] = c.g;
                imageData[i * 3 + 2] = c.b;
            }
            
            // Write the image to disk
            stbi_write_png(filename.c_str(), config.width, config.height, 3, imageData.data(), config.width * 3);
        }
        
        // Save the heightmap to disk
        void saveHeightmap(const std::string filename) {
            std::vector<uint8_t> imageData(config.width * config.height);
            
            for (size_t i = 0; i < world.elevation.size(); i++) {
                float v = (world.elevation[i] + 1.0f) * 0.5f;
                v = std::max(0.0f, std::min(1.0f, v));
                imageData[i] = static_cast<uint8_t>(v * 255.0f);
            }
            
            int result = stbi_write_png(filename.c_str(), config.width, config.height, 1, imageData.data(), config.width);
            
            if (!result) {
                std::cout << "ERROR SAVING HEIGHTMAP!!" << std::endl;
            }
        }
        
        // Getter
        const Config& getConfig() const { return config; }
        int getWidth() const { return config.width; }
        int getHeight() const { return config.height; }
        WorldData getWorld() const { return world; }
        std::vector<float> getElevation() const { return world.elevation; }
        std::vector<float> getMoisture() const { return world.moisture; }
        std::vector<float> getTemperature() const { return world.temperature; }
};

int main(int argc, char* argv[]) {
    try {
        // Edit the default values with the arguments passed from cmd
        Config config = parseArgs(argc, argv);
        ensureDirectory(config.output_dir);
        
        // Create generator obj
        terrainGenerator generator(config);
        
        // Start disk timer
        ScopedTimer timer("Save to disk");
        
        std::string terrainPath   = config.output_dir + "/" + config.output_file + ".png";
        std::string regionPath    = config.output_dir + "/" + config.output_file + "_REGION.png";
        std::string heightmapPath = config.output_dir + "/" + config.output_file + "_HEIGHTMAP.png";
        
        generator.saveToImage(terrainPath);
        generator.saveRegionMap(regionPath);
        generator.saveHeightmap(heightmapPath);
        
        std::cout << "Filename: " << config.output_file << std::endl;
        std::cout << "Saved to: " << config.output_dir  << "/" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <exception>
#include <iostream>
#include <optional>
#include <vector>
#include <iterator>
#include <ctime>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <random>
#include <thread>

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
    std::optional<int> seed;
    float frequency         = 0.003f;
    int octaves             = 5;
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -w, --width <int>       Map width  (default 1920)\n"
              << "  -h, --height <int>      Map height (default 1080)\n"
              << "  -o, --output <path>     Output PNG filename (default date)\n"
              << "  -s, --seed <int>        Noise seed (default 1337)\n"
              << "  -f, --frequency <float> Base noise frequency (default 0.0003)\n"
              << "      --octaves <int>     Fractal Octaves (default: 5)\n"
              << "      --help              Show this message\n";  
}

std::string getSystemDate() {
    std::time_t time = std::time({});
    char timeString[std::size("yyyy-mm-ddThh:mm:ssZ")];
    std::strftime(std::data(timeString), std::size(timeString), "%FT%TZ", std::localtime(&time));
            
    return timeString;
}

Config parseArgs(int argc, char* argv[]) {
    Config config;
           
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
                
        auto nextValue = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + flag);
            }
            return argv[++i];
        };
                
        if (arg == "-w" || arg == "--width") {
            config.width = std::stoi(nextValue(arg));
        } 
        else if (arg == "-h" || arg == "--height") {
            config.height = std::stoi(nextValue(arg));
        }
        else if (arg == "-o" || arg == "--output") {
            config.output_file = nextValue(arg);
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
        else if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
            
    if (config.width  <= 0) config.width  = 1920;
    if (config.height <= 0) config.height = 1080;
           
    if (config.output_file.empty()) {
        config.output_file = getSystemDate();
    }

    if (!config.seed.has_value()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1, 1000000);
        config.seed = dist(gen);
        std::cout << "no seed specified. using generated seed " << std::endl;
    }
    std::cout << "Seed: " << config.seed.value() << std::endl;
    config.output_file += "_S_";
    config.output_file += std::to_string(config.seed.value());
        
    return config;
}

class terrainGenerator {
    private:
        struct WorldData {
            std::vector<float> elevation;
            std::vector<float> moisture;
            std::vector<float> temperature;
        };
        struct Color { uint8_t r, g, b; };
          
        WorldData world;        
        Config config;
                              
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
        
        // Calculate distance between pixel and border
        float calculateFalloff(int x, int y, int width, int height) {
            // Normalize coords [-1, 1]
            float nx = (2.0f * x / width)  - 1.0f;
            float ny = (2.0f * y / height) - 1.0f;
            
            // Distance from center
            float distance = 1.0f - (1.0f - nx * nx) * (1.0f - ny * ny);
            
            // Clamp distance [0, 1]
            distance = std::min(distance, 1.0f);
            
            // Cubic fallout curve
            // Used to make sure the coastline isn't a small circle in the middle
            return distance * distance * distance;
        }
                
    public:   
        terrainGenerator(const Config& cfg) : config(cfg) {
            
            ScopedTimer timer("Generation");
            world = generate(config.width, config.height, config.seed.value(), config.frequency, config.octaves);
            
            std::cout << "TOT: " << world.elevation.size() << std::endl;
        }
        
        WorldData generate(int width, int height, int seed, float frequency, int octaves) {
            FastNoiseLite elevNoise;
            elevNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            elevNoise.SetSeed(seed);
            elevNoise.SetFrequency(frequency);
            elevNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
            elevNoise.SetFractalOctaves(octaves);
            elevNoise.SetFractalLacunarity(2.0f);
            elevNoise.SetFractalGain(0.5f);            
            
            FastNoiseLite detail;
            detail.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            detail.SetSeed(seed + 1); // Offset so it's not identical to elevation'
            detail.SetFrequency(0.013f);
            detail.SetFractalType(FastNoiseLite::FractalType_FBm);
            detail.SetFractalOctaves(3);
            
            FastNoiseLite moistureNoise;
            moistureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            moistureNoise.SetFrequency(0.0015f);
            moistureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
            moistureNoise.SetFractalOctaves(4);
            moistureNoise.SetSeed(1337);
            
            FastNoiseLite tempNoise;
            tempNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            tempNoise.SetFrequency(0.002f);
            tempNoise.SetSeed(9001);
            
            WorldData world;
            world.elevation.resize(width * height);
            world.moisture.resize(width * height);
            world.temperature.resize(width * height);
            
            auto computeRows = [&](int yStart, int yEnd) {
                for (int y = yStart; y < yEnd; y++) {
                    int index = y * width;
                    for (int x = 0; x < width; x++) {
                        // Elevation
                        float e = elevNoise.GetNoise( (float)x, (float)y);
                        e += detail.GetNoise((float)x, (float)y) * 0.08f;
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
        
        void saveToImage(const std::string filename) {
            std::vector<uint8_t> imageData(config.width * config.height * 3);
            
            for (size_t i = 0; i < world.elevation.size(); i++) {
                Color c = getBiomeColor(world.elevation[i], world.moisture[i], world.temperature[i]);
                
                imageData[i * 3 + 0] = c.r;
                imageData[i * 3 + 1] = c.g;
                imageData[i * 3 + 2] = c.b;
            }
            
            int result = stbi_write_png(filename.c_str(), config.width, config.height, 3, imageData.data(), config.width * 3);
            
            if (result) {
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR SAVING THE IMAGE" << std::endl;
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
        Config config = parseArgs(argc, argv);
        
        terrainGenerator generator(config);
        ScopedTimer timer("Save to disk");
        generator.saveToImage(config.output_file);
        std::cout << "Filename: " << config.output_file << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

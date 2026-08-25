#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <vector>
#include <iterator>
#include <ctime>
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

class terrainGenerator {
    private:
        int width;
        int height;
        struct WorldData {
            std::vector<float> elevation;
            std::vector<float> moisture;
            std::vector<float> temperature;
        };
        WorldData world;
        struct Color { uint8_t r, g, b; };
                
        int validateWidth(std::string userInput) {
            int width = std::stoi(userInput);
            
            return (width > 0) ? width : 1920;
        } 

        int validateHeight(std::string userInput) {
            int height = std::stoi(userInput);
            
            return (height > 0) ? height : 1080;
        }
        
        Color lerpColor(Color a, Color b, float t) {
            return {
                static_cast<uint8_t>(a.r + (b.r - a.r) *t),
                static_cast<uint8_t>(a.g + (b.g - a.g) *t),
                static_cast<uint8_t>(a.b + (b.b - a.b) *t)
            };
        }
        
        Color getColorForValue(float normalized) {
            struct Stop { float pos; Color color; };
            static const Stop stops[] {
                 {0.0f, {30,   60, 140}},  // Deep water
                 {0.35f, {60,  110, 200}}, // Shallow water
                 {0.45f, {210, 200, 150}}, // Sand
                 {0.50f, {70,  140,  60}}, // Grass
                 {0.70f, {110,  90,  70}}, // Mountain
                 {0.85f, {255, 255, 255}}, // Snow  
            };
            constexpr int n = sizeof(stops) / sizeof(Stop);
            
            for (int i = 0; i < n - 1; i++) {
                if (normalized >= stops[i].pos && normalized <= stops[i + 1].pos) {
                    float t = (normalized - stops[i].pos) / (stops[i + 1].pos - stops[i].pos);
                    return lerpColor(stops[i].color, stops[i+1].color, t);
                }
            }
            
            return normalized < stops[0].pos ? stops[0].color : stops[n - 1].color;
        }
        
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
        std::string getDate() {
            std::time_t time = std::time({});
            char timeString[std::size("yyyy-mm-ddThh:mm:ssZ")];
            std::strftime(std::data(timeString), std::size(timeString), "%FT%TZ", std::localtime(&time));
            
            return timeString;
        }
        
        terrainGenerator(int argc, char* argv[]) {
            std::string rawWidth  = (argc > 1) ? argv[1] : "1920";
            std::string rawHeight = (argc > 2) ? argv[2] : "1080";
            
            width  = validateWidth(rawWidth);
            height = validateHeight(rawHeight);
            
            ScopedTimer timer("Generation");
            world = generate(width, height);
            
            std::cout << "TOT Elevation: " << world.elevation.size() << std::endl;
        }
        
        WorldData generate(int width, int height) {
            FastNoiseLite elevNoise;
            elevNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            elevNoise.SetFrequency(0.003f);
            elevNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
            elevNoise.SetFractalOctaves(5);
            elevNoise.SetFractalLacunarity(2.0f);
            elevNoise.SetFractalGain(0.5f);            
            
            FastNoiseLite detail;
            detail.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
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
            
            int index = 0;
            std::vector<float> noiseData(width * height);
            
            for (int y = 0; y < height; y++) {
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
            
            return world;
        }
        
        void saveToImage(const std::string filename) {
            std::vector<uint8_t> imageData(width * height * 3);
            
            for (size_t i = 0; i < world.elevation.size(); i++) {
                Color c = getBiomeColor(world.elevation[i], world.moisture[i], world.temperature[i]);
                
                imageData[i * 3 + 0] = c.r;
                imageData[i * 3 + 1] = c.g;
                imageData[i * 3 + 2] = c.b;
            }
            
            int result = stbi_write_png(filename.c_str(), width, height, 3, imageData.data(), width * 3);
            
            if (result) {
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR SAVING THE IMAGE" << std::endl;
            }
        }
        
        // Getters
        int getWidth()  const { return width;  }
        int getHeight() const { return height; }
        WorldData getWorld() const { return world; }
        std::vector<float> getElevation() const { return world.elevation; }
        std::vector<float> getMoisture() const { return world.moisture; }
        std::vector<float> getTemperature() const { return world.temperature; }
        
        void print() {
            std::cout << "Validated WIDTH:  " << width  << std::endl;
            std::cout << "Validated HEIGHT: " << height << std::endl;
        }
};

int main(int argc, char* argv[]) {
    /*
    for (int i=0; i<argc; i++) {
        std::cout << "Argument " << i << " " << argv[i] << std::endl; 
    }
    */

    terrainGenerator generator(argc, argv);    
    
    generator.print();
    
    std::string filename  = (argc > 3) ? argv[3] : generator.getDate();
    ScopedTimer timer("Saving to disk");
    generator.saveToImage(filename);
    
    std::cout << "Created file: " << filename << ".png" << std::endl;
    
    return 0;
}

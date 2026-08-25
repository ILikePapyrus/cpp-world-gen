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
        std::vector<float> terrain;        
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
            terrain = generate(width, height);
            
            /*
            for (float element : terrain) {
                std::cout << element << std::endl;
            }
            */
            
            std::cout << "TOT: " << terrain.size() << std::endl;;
        }
        
        std::vector<float> generate(int width, int height) {
            FastNoiseLite noise;
            // Generator settings
            noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
            noise.SetFrequency(0.0013f);
            noise.SetFractalType(FastNoiseLite::FractalType_FBm);
            noise.SetFractalOctaves(5);
            noise.SetFractalLacunarity(2.0f);
            noise.SetFractalGain(0.5f);
            
            FastNoiseLite detail;
            detail.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
            detail.SetFrequency(0.013f);
            detail.SetFractalType(FastNoiseLite::FractalType_FBm);
            detail.SetFractalOctaves(3);
            
            int index = 0;
            std::vector<float> noiseData(width * height);
            
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    float value   = noise.GetNoise( (float)x, (float)y);
                    float detailV = detail.GetNoise((float)x, (float)y);
                    float falloff = calculateFalloff(x, y, width, height);
                    
                    value += detailV * 0.08f;
                    value -= falloff * 2.0f;                                     // Pull edges toward deep water
                    value  = std::max(-1.0f, std::min(1.0f, value)); // Keep the value in range
                    
                    noiseData[index++] = value;
                }
            }
            
            return noiseData;
        }
        
        void saveToImage(const std::string filename) {
            std::vector<uint8_t> imageData(width * height * 3);
            
            for (size_t i = 0; i < terrain.size(); i++) {
                float normalized = (terrain[i] + 1.0f) * 0.5f;
                Color c = getColorForValue(normalized);
                
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
        std::vector<float> getTerrain() const { return terrain; }
        
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

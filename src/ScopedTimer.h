#pragma once

// Include
#include <chrono>
#include <iostream>
#include <string>
#include <utility>

class ScopedTimer
{
public:
    // Constructor
    explicit ScopedTimer(std::string label) : label(std::move(label)), start(std::chrono::high_resolution_clock::now()) {}

    // Destructor
    ~ScopedTimer()
    {
        std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
        // Print time
        std::cout << label << ": " << elapsed.count() << " seconds" << std::endl;
    }

    ScopedTimer(const ScopedTimer&)            = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    // Private Attributes
    std::string label;
    std::chrono::high_resolution_clock::time_point start;
};
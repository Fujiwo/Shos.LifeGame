#pragma once

#include <iostream>
#include <chrono>
#include <ctime>

namespace Shos {

class stopwatch
{
    const std::chrono::time_point<std::chrono::system_clock> start; // Start time

public:
    void (*output)(double elapsed) = show_result;

    stopwatch() : start(std::chrono::system_clock::now())
    {}

    virtual ~stopwatch()
    {
        const auto end     = std::chrono::system_clock::now();  // End time
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
        show_result(elapsed);
    }

private:
    static void show_result(double elapsed)
    {
        std::cout << "(" << elapsed << "s.)" << std::endl;
    }
};

} // namespace Shos

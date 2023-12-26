#pragma once

#include <iostream>
#include <chrono>

namespace Shos {

class stopwatch
{
    bool                                               has_started;
    std::chrono::time_point<std::chrono::system_clock> start_time;

public:
    bool is_running() const
    { return has_started; }

    stopwatch() : has_started(false)
    {}

    void start()
    {
        has_started = true;
        start_time  = std::chrono::system_clock::now();
    }

    double get_elapsed() const
    {
        if (!has_started)
            return 0.0;

        const auto end_time = std::chrono::system_clock::now();  // End time
        const auto elapsed  = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() / 1000.0;
        return elapsed;
    }
};

class stopwatch_viewer
{
    stopwatch stopwatch;

public:
    void (*output)(double elapsed) = show_result;

    stopwatch_viewer()
    { stopwatch.start(); }

    virtual ~stopwatch_viewer()
    { show_result(stopwatch.get_elapsed()); }

private:
    static void show_result(double elapsed)
    { std::cout << "(" << elapsed << "s.)" << std::endl; }
};

} // namespace Shos

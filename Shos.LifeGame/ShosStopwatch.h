#pragma once

#include <iostream>
#include <chrono>

namespace Shos {

/// <summary>Measures the elapsed time in seconds.</summary>
class stopwatch
{
    bool                                  has_started = false;
    std::chrono::steady_clock::time_point start_time;

public:
    bool is_running() const
    { return has_started; }

    /// <summary>Starts, or restarts from 0 if already running.</summary>
    void start()
    {
        has_started = true;
        start_time  = std::chrono::steady_clock::now();
    }

    /// <returns>Seconds since start, or 0 if not started.</returns>
    double get_elapsed() const
    {
        if (!has_started)
            return 0.0;

        const auto end_time = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(end_time - start_time).count();
    }
};

/// <summary>Prints the time from its construction to its destruction to the standard output.</summary>
class stopwatch_viewer final
{
    stopwatch watch;

public:
    stopwatch_viewer()
    { watch.start(); }

    ~stopwatch_viewer()
    { show_result(watch.get_elapsed()); }

private:
    static void show_result(double elapsed)
    { std::cout << "(" << elapsed << "s.)" << std::endl; }
};

} // namespace Shos

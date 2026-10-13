#pragma once

#include <iostream>
#include <chrono>

namespace Shos {

class stopwatch
{
    bool                                  has_started = false;
    std::chrono::steady_clock::time_point start_time;

public:
    bool is_running() const
    { return has_started; }

    void start()
    {
        has_started = true;
        start_time  = std::chrono::steady_clock::now();
    }

    double get_elapsed() const
    {
        if (!has_started)
            return 0.0;

        const auto end_time = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(end_time - start_time).count();
    }
};

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

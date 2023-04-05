#ifndef STOPWATCH_HPP
#define STOPWATCH_HPP

// C++ libraries
#include <chrono>
#include <ctime>
#include <ostream>

class Stopwatch {
public:
    // Constructor
    Stopwatch ();

    // Methods
    void start ();
    void stop ();
    void reset ();

    double get_time_taken_in_seconds ();

    uint64_t get_num_starts ();

private:
    // Variables
    std::chrono::time_point<std::chrono::steady_clock> m_begin;
    std::chrono::duration<uint64_t, std::nano> m_time_taken;
    uint64_t m_num_starts;
};

#endif // STOPWATCH_HPP

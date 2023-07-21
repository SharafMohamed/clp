// C libraries
#include <unistd.h>

// Catch2
#include "../submodules/Catch2/single_include/catch2/catch.hpp"

// Project headers
#include "../src/Stopwatch.hpp"

TEST_CASE("Stopwatch", "[Stopwatch]") {
    Stopwatch stopwatch;

    SECTION("Test if initialized with 0.0") {
        double time_taken = stopwatch.get_time_taken_in_seconds();
        REQUIRE(time_taken == 0.0);
    }

    SECTION("Test reset()") {
        // Measure some work
        stopwatch.start();
        sleep(1);
        stopwatch.stop();

        stopwatch.reset();

        double time_taken = stopwatch.get_time_taken_in_seconds();
        REQUIRE(time_taken == 0.0);
    }

    SECTION("Test single measurement") {
        // Measure some work
        stopwatch.start();
        sleep(1);
        stopwatch.stop();

        double time_taken = stopwatch.get_time_taken_in_seconds();
        REQUIRE(time_taken >= 1.0);
        REQUIRE(time_taken < 1.1);
    }
}
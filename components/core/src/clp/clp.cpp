#include <string>

#include <spdlog/spdlog.h>

#include "run.hpp"

#include <glog/logging.h>

int main (int argc, const char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    std::string archive_path;
    try {
        return clp::run(argc, argv);
    } catch (std::string const err) {
        SPDLOG_ERROR(err.c_str());
        return 1;
    }
}

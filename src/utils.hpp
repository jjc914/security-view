#ifndef UTILS_HPP
#define UTILS_HPP

#include <fstream>
#include <string>
#include <chrono>
#include <ctime>

std::string current_date_time_str() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y.%m.%d.%H.%M.%S");
    return ss.str();
}

std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        std::cerr << "error: could not open file " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

#endif
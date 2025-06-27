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

bool ends_with(const std::string& value, const std::string& ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}


float dot(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        std::cerr << "error: dot product vector sizes do not match.\n";
        return 0;
    }

    float s = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        s += a[i] * b[i];
    }
    return s;
}

#endif
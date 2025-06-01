#ifndef TYPES_HPP
#define TYPES_HPP

#include <opencv2/opencv.hpp>

#include <chrono>

struct FrameEntry {
    cv::Mat frame;
    std::chrono::time_point<std::chrono::steady_clock> steady_time;
};

#endif
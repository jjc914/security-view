#ifndef TYPES_HPP
#define TYPES_HPP

#include <opencv2/opencv.hpp>

#include <chrono>
#include <array>

struct FrameEntry {
    cv::Mat frame;
    std::chrono::time_point<std::chrono::steady_clock> steady_time;
};

struct FaceObject {
    float prob;
    cv::Rect rect;
    std::array<cv::Point2f, 5> landmarks;
};

struct EmbeddingEntry {
    std::string name;
    std::vector<float> embedding;
};

#endif
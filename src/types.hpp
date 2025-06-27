#ifndef TYPES_HPP
#define TYPES_HPP

#include <opencv2/opencv.hpp>
#include <sqlite3.h>

#include <chrono>
#include <array>
#include <functional>

struct FrameEntry {
    cv::Mat frame;
    std::chrono::time_point<std::chrono::steady_clock> steady_time;
};

struct FaceObject {
    float prob;
    cv::Rect rect;
    std::array<cv::Point2f, 5> landmarks;
};

struct DetectionResult {
    cv::Mat frame;
    std::vector<FaceObject> faces;
};

struct EmbeddingEntry {
    std::string name;
    std::vector<float> embedding;
};

struct SQLQuery {
    enum class Priority {
        HIGH = 2,
        MEDIUM = 1,
        LOW = 0
    };
    struct Comparator {
        bool operator()(const SQLQuery& a, const SQLQuery& b) const {
            return a.priority < b.priority;
        }
    };
    Priority priority = Priority::MEDIUM;
    std::string sql;
    std::function<void(sqlite3_stmt*)> on_bind;
    std::function<void(sqlite3_stmt*)> on_row;
    std::function<void()> on_done;
};

#endif
#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <opencv2/opencv.hpp>
#include <net.h>

#include "types.hpp"

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <chrono>
#include <queue>
#include <condition_variable>


static const std::array<cv::Point2f, 5> g_EMBEDDING_REFERENCE = {
    cv::Point2f(38.2946f, 51.6963f),
    cv::Point2f(73.5318f, 51.5014f),
    cv::Point2f(56.0252f, 71.7366f),
    cv::Point2f(41.5493f, 92.3655f),
    cv::Point2f(70.7299f, 92.2041f)
};

extern ncnn::Net g_retinaface_net;
extern ncnn::Net g_mobilefacenet_net;

extern std::atomic<int> g_frame_count;
extern std::atomic<int> g_fps;
extern std::atomic<bool> g_should_record;
extern std::atomic<bool> g_should_reload_db;

extern std::atomic<bool> g_exit_fps_thread;
extern std::atomic<bool> g_exit_server_thread;
extern std::atomic<bool> g_exit_db_thread;
extern std::atomic<bool> g_exit_recording_thread;
extern std::atomic<bool> g_exit_detection_thread;
extern std::atomic<bool> g_exit_embedding_thread;
extern std::atomic<bool> g_exit_main_thread;

extern cv::Mat g_frame;
extern std::mutex g_frame_mutex;

extern cv::Mat g_streaming_buffer;                     // read: server
extern std::mutex g_streaming_buffer_mutex;            // write: main

extern cv::Mat g_annotated_streaming_buffer;           // read: server
extern std::mutex g_annotated_streaming_buffer_mutex;  // write: embedding

// recording thread
extern std::queue<FrameEntry> g_recording_buffer;      // read: recording
extern std::mutex g_recording_buffer_mutex;            // write: main

extern std::queue<DetectionResult> g_embedding_buffer; // read: embedding
extern std::mutex g_embedding_buffer_mutex;            // write: detection
extern std::condition_variable g_embedding_buffer_cv;

                                                       // read: server
extern std::unordered_map<std::string, std::chrono::time_point<std::chrono::steady_clock>> g_valid_sessions;
extern std::mutex g_valid_sessions_mutex;              // write: server

                                                       // read: db
extern std::priority_queue<SQLQuery, std::vector<SQLQuery>, SQLQuery::Comparator> g_sql_queue;
extern std::mutex g_sql_queue_mutex;                   // write: server
extern std::condition_variable g_sql_queue_cv;

#endif
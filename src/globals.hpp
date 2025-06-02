#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <opencv2/opencv.hpp>

#include "types.hpp"

#include <atomic>
#include <mutex>
#include <queue>

extern std::atomic<int> g_frame_count;
extern std::atomic<int> g_fps;
extern std::atomic<bool> g_should_stream;
extern std::atomic<bool> g_should_record;

extern std::atomic<bool> g_exit_fps_thread;
extern std::atomic<bool> g_exit_server_thread;
extern std::atomic<bool> g_exit_recording_thread;
extern std::atomic<bool> g_exit_detection_thread;
extern std::atomic<bool> g_exit_embedding_thread;
extern std::atomic<bool> g_exit_main_thread;

extern cv::Mat g_frame;
extern std::mutex g_frame_mutex;

extern cv::Mat g_streaming_buffer;
extern std::mutex g_streaming_buffer_mutex;

extern std::queue<FrameEntry> g_recording_buffer;
extern std::mutex g_recording_buffer_mutex;

extern std::queue<cv::Mat> g_embedding_buffer;
extern std::mutex g_embedding_buffer_mutex;

// debug
extern cv::Mat g_retina_debug_buffer;
extern std::mutex g_retina_debug_buffer_mutex;

#endif
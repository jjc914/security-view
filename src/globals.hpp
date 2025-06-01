#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include "types.hpp"

#include <atomic>
#include <mutex>
#include <queue>
#include <opencv2/opencv.hpp>

extern std::atomic<int> g_frame_count;
extern std::atomic<int> g_fps;
extern std::atomic<bool> g_should_stream;
extern std::atomic<bool> g_is_head_detected;

extern std::atomic<bool> g_exit_fps_thread;
extern std::atomic<bool> g_exit_server_thread;
extern std::atomic<bool> g_exit_recording_thread;
extern std::atomic<bool> g_exit_detection_thread;
extern std::atomic<bool> g_exit_main_thread;

extern cv::Mat g_frame;
extern std::mutex g_frame_mutex;

extern std::queue<FrameEntry> g_recording_buffer;
extern std::mutex g_recording_buffer_mutex;

#endif
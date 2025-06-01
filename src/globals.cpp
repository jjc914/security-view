#include "globals.hpp"

std::atomic<int> g_frame_count(0);
std::atomic<int> g_fps(0);
std::atomic<bool> g_should_stream(false);
std::atomic<bool> g_is_head_detected(false);

std::atomic<bool> g_exit_fps_thread(false);
std::atomic<bool> g_exit_server_thread(false);
std::atomic<bool> g_exit_recording_thread(false);
std::atomic<bool> g_exit_detection_thread(false);
std::atomic<bool> g_exit_main_thread(false);

cv::Mat g_frame;
std::mutex g_frame_mutex;

std::queue<FrameEntry> g_recording_buffer;
std::mutex g_recording_buffer_mutex;
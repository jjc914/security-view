#include "globals.hpp"

#include <atomic>

std::atomic<int> g_frame_count(0);
std::atomic<int> g_fps(0);
std::atomic<bool> g_should_stream(false);
std::atomic<bool> g_should_record(false);

std::atomic<bool> g_exit_fps_thread(false);
std::atomic<bool> g_exit_server_thread(false);
std::atomic<bool> g_exit_recording_thread(false);
std::atomic<bool> g_exit_detection_thread(false);
std::atomic<bool> g_exit_embedding_thread(false);
std::atomic<bool> g_exit_main_thread(false);

cv::Mat g_frame;
std::mutex g_frame_mutex;

cv::Mat g_streaming_buffer;
std::mutex g_streaming_buffer_mutex;

std::queue<FrameEntry> g_recording_buffer;
std::mutex g_recording_buffer_mutex;

std::queue<RetinaResult> g_embedding_buffer;
std::mutex g_embedding_buffer_mutex;

cv::Mat g_annotated_streaming_buffer;
std::mutex g_annotated_streaming_buffer_mutex;

// debug
cv::Mat g_retina_debug_buffer_1;
std::mutex g_retina_debug_buffer_1_mutex;
cv::Mat g_retina_debug_buffer_2;
std::mutex g_retina_debug_buffer_2_mutex;
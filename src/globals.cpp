#include "globals.hpp"

ncnn::Net g_retinaface_net;
ncnn::Net g_mobilefacenet_net;

std::atomic<int> g_frame_count(0);
std::atomic<int> g_fps(0);
std::atomic<bool> g_should_record(false);
std::atomic<bool> g_should_reload_db(false);

std::atomic<bool> g_exit_fps_thread(false);
std::atomic<bool> g_exit_server_thread(false);
std::atomic<bool> g_exit_db_thread(false);
std::atomic<bool> g_exit_recording_thread(false);
std::atomic<bool> g_exit_detection_thread(false);
std::atomic<bool> g_exit_embedding_thread(false);
std::atomic<bool> g_exit_main_thread(false);

cv::Mat g_frame;
std::mutex g_frame_mutex;

cv::Mat g_streaming_buffer;
std::mutex g_streaming_buffer_mutex;

cv::Mat g_annotated_streaming_buffer;
std::mutex g_annotated_streaming_buffer_mutex;

std::queue<FrameEntry> g_recording_buffer;
std::mutex g_recording_buffer_mutex;

std::queue<DetectionResult> g_embedding_buffer;
std::mutex g_embedding_buffer_mutex;
std::condition_variable g_embedding_buffer_cv;

std::unordered_map<std::string, std::chrono::time_point<std::chrono::steady_clock>> g_valid_sessions;
std::mutex g_valid_sessions_mutex;

std::priority_queue<SQLQuery, std::vector<SQLQuery>, SQLQuery::Comparator> g_sql_queue;
std::mutex g_sql_queue_mutex;
std::condition_variable g_sql_queue_cv;
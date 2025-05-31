#include <opencv2/opencv.hpp>
#include <httplib.h>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>

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
        std::cerr << "Could not open file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

// fps thread
std::atomic<int> g_frame_count(0);
std::atomic<int> g_fps(0);

std::atomic<bool> g_exit_fps_thread(false);
void fps_thread_func() {
    g_exit_fps_thread.store(false);
    while (!g_exit_fps_thread.load()) {
        int start_count = g_frame_count.load();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        int end_count = g_frame_count.load();
        g_fps.store(end_count - start_count);
    }
}

// server thread
cv::Mat g_frame;
std::mutex g_frame_mutex;
std::atomic<bool> g_should_stream(false);
void server_listening_thread_func(httplib::Server& server) {
    std::cout << "[server] info: started server listening thread.\n";
    std::string ip = "0.0.0.0";
    uint16_t port = 8080;

    server.Get("/video", [&](const httplib::Request& req, httplib::Response& res) {
        // set the mime type to indicate multipart mjpeg stream
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");

        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&](size_t offset, httplib::DataSink &sink) -> bool {
                // write data
                std::vector<uchar> buf;
                std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };

                while (sink.is_writable()) {
                    { std::lock_guard<std::mutex> lock(g_frame_mutex);
                        if (g_frame.empty()) {
                            continue;
                        }
                        
                        cv::imencode(".jpg", g_frame, buf, params);
                    }
                    // Encode the captured frame as JPEG.

                    // Build the multipart frame header.
                    std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                        std::to_string(buf.size()) + "\r\n\r\n";
                    // Write the header, JPEG data, and a trailing newline.
                    if (!sink.write(header.c_str(), header.size()))
                        break;
                    if (!sink.write(reinterpret_cast<const char*>(buf.data()), buf.size()))
                        break;
                    if (!sink.write("\r\n", 2))
                        break;
                }
                return true;
            }
        );
    });

    server.Get("/", [](const httplib::Request &req, httplib::Response &res) {
        res.set_content(read_file("src/web/index.html"), "text/html");
    });

    server.Post("/togglestream", [](const httplib::Request& req, httplib::Response& res) {
        g_should_stream.store(!g_should_stream.load());
    });

    std::cout << "[server] info: server is running on: http://"<< ip << ":" << port << "." << std::endl;
    server.listen("0.0.0.0", 8080);
}

// recording thread
struct FrameEntry {
    cv::Mat frame;
    std::chrono::time_point<std::chrono::steady_clock> steady_time;
};

std::queue<FrameEntry> g_recording_queue;
std::mutex g_recording_queue_mutex;
std::atomic<bool> g_exit_recording_thread(false);
std::atomic<bool> g_is_head_detected(false);
void recording_thread_func(const cv::Size frame_size) {
    // parameters
    std::cout << "[rec] info: started recording thread.\n";
    const float activate_time = 1; // seconds
    const float deactivate_time = 2;
    const float prerecord_buffer = 2;

    g_exit_recording_thread.store(false);

    const int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');

    bool is_first_found = false;
    std::chrono::time_point<std::chrono::steady_clock> first_found_time;
    while (!g_exit_recording_thread.load()) {
        const int fps = g_fps.load();
        if (fps < 1) continue;

        // remove expired frames
        const auto steady_now = std::chrono::steady_clock::now();
        { std::lock_guard<std::mutex> lock(g_recording_queue_mutex);
            if (!g_recording_queue.empty()) {
                while ((steady_now - g_recording_queue.front().steady_time) >= std::chrono::duration<float>(prerecord_buffer)) {
                    g_recording_queue.pop();
                }
            }
        }

        // check if should start recording
        if (g_is_head_detected.load()) {
            if (!is_first_found) {
                // first head detected, start the timer
                std::cout << "starting timer" << std::endl;
                first_found_time = std::chrono::steady_clock::now();
                is_first_found = true;
            }

            // if time 
            if ((steady_now - first_found_time) >= std::chrono::duration<float>(activate_time)) {
                // activate recording
                std::cout << "writing recording to " << "rec/" + current_date_time_str() + ".avi" << std::endl;
                cv::VideoWriter video_writer = cv::VideoWriter("rec/" + current_date_time_str() + ".avi", fourcc, fps, frame_size, true);
                if (!video_writer.isOpened()) {
                    std::cerr << "error: could not open video writer." << std::endl;
                }
                // keep recording until not
                std::chrono::time_point<std::chrono::steady_clock> last_found_time = first_found_time;
                while ((std::chrono::steady_clock::now() - last_found_time) <= std::chrono::duration<float>(deactivate_time)) {
                    if (g_is_head_detected.load()) {
                        last_found_time = std::chrono::steady_clock::now();
                    }
                    // write all buffered data
                    { std::unique_lock<std::mutex> lock(g_recording_queue_mutex, std::defer_lock);
                        while (!g_recording_queue.empty()) {
                            lock.lock();
                            cv::Mat frame = g_recording_queue.front().frame;
                            g_recording_queue.pop();
                            lock.unlock();
                            video_writer.write(frame);
                        }
                    }
                }

                // stop recording
                std::cout << "stop recording" << std::endl;
                video_writer.release();
            }
        } else {
            is_first_found = false;
        }
    }
}

int main() {
    // parameters
    const float target_fps = 20; // sets a maximum fps

    // initialization
    httplib::Server server;

    std::string pipeline =
        "libcamerasrc ! "
        "video/x-raw,width=640,height=480,format=RGB,framerate=30/1 ! "
        "videoconvert ! "
        "appsink sync=false max-buffers=1 drop=true";

    cv::VideoCapture video_capture(pipeline, cv::CAP_GSTREAMER);
    if (!video_capture.isOpened()) {
        std::cerr << "error: could not open camera." << std::endl;
        return -1;
    }
    std::cout << "[main] info: opened camera.\n";

    cv::CascadeClassifier head_cascade;
    if (!head_cascade.load("res/haarcascade_frontalface_default.xml")) {
        std::cerr << "error: could not load Haar cascade for head detection." << std::endl;
        return -1;
    }
    std::cout << "[main] info: loaded Haar cascade.\n";

    // define capture info
    const uint32_t frame_width = static_cast<int>(video_capture.get(cv::CAP_PROP_FRAME_WIDTH));
    const uint32_t frame_height = static_cast<int>(video_capture.get(cv::CAP_PROP_FRAME_HEIGHT));

    // fps calculations
    const std::chrono::milliseconds target_frame_duration(1000 / (int)target_fps);
    auto fps_start = std::chrono::steady_clock::now();

    // start threads
    std::thread server_listening_thread = std::thread(server_listening_thread_func, std::ref(server));
    std::thread fps_thread = std::thread(fps_thread_func);
    std::thread recording_thread = std::thread(recording_thread_func, cv::Size(frame_width, frame_height));
    
    std::cout << "[main] info: starting frame recording.\n";
    cv::Mat frame, gray;
    while (true) {
        // frame start info
        const auto frame_start = std::chrono::steady_clock::now();

        // capture frame
        video_capture >> frame;
        if (frame.empty()) {
            std::cout << "warning: no valid frame, sleeping 200ms and retrying" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // convert to grayscale for detection
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::equalizeHist(gray, gray);

        // detect heads
        std::vector<cv::Rect> heads;
        head_cascade.detectMultiScale(gray, heads, 1.1, 3, 0, cv::Size(30, 30));

        if (heads.size() > 0) {
            // square around heads
            for (const auto& head : heads) {
                cv::rectangle(frame, head, cv::Scalar(0, 255, 0), 2);
            }
            g_is_head_detected.store(true);
        } else {
            g_is_head_detected.store(false);
        }

        // update server
        if (g_should_stream.load()) {
            { std::lock_guard<std::mutex> lock(g_frame_mutex);
                g_frame = frame.clone();
            }
        }
        
        // end frame
        g_frame_count.fetch_add(1);
        const auto frame_end = std::chrono::steady_clock::now();

        // record frame
        { std::lock_guard<std::mutex> lock(g_recording_queue_mutex);
            g_recording_queue.push({ frame.clone(), frame_end });
        }

        // sleep for target fps
        auto frame_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);
        if (frame_elapsed < target_frame_duration) {
            std::this_thread::sleep_for(target_frame_duration - frame_elapsed);
        } else {

        }
    }

    video_capture.release();
    cv::destroyAllWindows();

    g_is_head_detected.store(false);
    g_exit_recording_thread.store(true);
    recording_thread.join();

    g_exit_fps_thread.store(true);
    fps_thread.join();

    server.stop();
    server_listening_thread.join();

    return 0;
}

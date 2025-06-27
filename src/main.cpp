#include <opencv2/opencv.hpp>

#include "utils.hpp"
#include "threads/fps.hpp"
#include "threads/server.hpp"
#include "threads/db.hpp"
#include "threads/recording.hpp"
#include "threads/detection.hpp"
#include "threads/embedding.hpp"

#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <csignal>

#include "globals.hpp"

void signal_handler(int signum) {
    std::cout << "[sig] info: recieved signal " << signum << ", exiting main thread.\n";
    g_exit_main_thread.store(true);
}

int main() {
    const float target_fps = 20; // sets a maximum fps

    // build video capture device
    std::string pipeline =
        "libcamerasrc ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink sync=false max-buffers=1 drop=true";
    cv::VideoCapture video_capture(pipeline, cv::CAP_GSTREAMER);
    if (!video_capture.isOpened()) {
        std::cerr << "[main] error: could not open camera." << std::endl;
        return -1;
    }
    std::cout << "[main] info: opened camera.\n";

    // define capture info
    const uint32_t frame_width = static_cast<int>(video_capture.get(cv::CAP_PROP_FRAME_WIDTH));
    const uint32_t frame_height = static_cast<int>(video_capture.get(cv::CAP_PROP_FRAME_HEIGHT));

    // fps calculations
    const std::chrono::milliseconds target_frame_duration(1000 / (int)target_fps);
    auto fps_start = std::chrono::steady_clock::now();

    // signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // init networks
    std::cout << "[main] info: found " << ncnn::get_gpu_count() << " gpus.\n";

    g_retinaface_net.opt.use_vulkan_compute = true;
    g_retinaface_net.opt.num_threads = 4;
    // https://github.com/nihui/ncnn-assets/tree/master/models
    if (g_retinaface_net.load_param("models/retinaface/mnet.25-opt.param") != 0 ||
            g_retinaface_net.load_model("models/retinaface/mnet.25-opt.bin") != 0) {
        std::cerr << "[main] error: failed to load RetinaFace model." << std::endl;
        return 0;
    }
    std::cout << "[main] info: RetinaFace model loaded successfully.\n";

    g_mobilefacenet_net.opt.use_vulkan_compute = true;
    g_mobilefacenet_net.opt.num_threads = 4;
    // https://github.com/liguiyuan/mobilefacenet-ncnn/tree/master/models
    if (g_mobilefacenet_net.load_param("models/mobilefacenet/mobilefacenet.param") != 0 ||
            g_mobilefacenet_net.load_model("models/mobilefacenet/mobilefacenet.bin") != 0) {
        std::cerr << "[main] error: failed to load MobileFaceNet model." << std::endl;
        return 0;
    }
    std::cout << "[main] info: MobileFaceNet model loaded successfully.\n";

    // start threads
    std::thread fps_thread = std::thread(fps_thread_func);
    std::thread db_thread = std::thread(db_thread_func);
    std::thread recording_thread = std::thread(recording_thread_func, cv::Size(frame_width, frame_height));
    std::thread detection_thread = std::thread(detection_thread_func);
    std::thread embedding_thread = std::thread(embedding_thread_func);
    std::thread server_thread = std::thread(server_thread_func);

    std::cout << "[main] info: starting frame recording.\n";
    cv::Mat frame, gray;
    while (!g_exit_main_thread.load()) {
        // frame start info
        const auto frame_start = std::chrono::steady_clock::now();

        // capture frame
        video_capture >> frame;
        if (frame.empty()) {
            std::cout << "[main] warning: no valid frame, sleeping 200ms and retrying" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        
        // end frame
        g_frame_count.fetch_add(1);
        const auto frame_end = std::chrono::steady_clock::now();

        // record frame
        { std::lock_guard<std::mutex> lock(g_frame_mutex);
            g_frame = frame.clone();
        }
        
        { std::lock_guard<std::mutex> lock(g_recording_buffer_mutex);
            g_recording_buffer.push({ frame.clone(), frame_end });
        }

        { std::lock_guard<std::mutex> lock(g_streaming_buffer_mutex);
            g_streaming_buffer = std::move(frame);
        }

        // sleep for target fps
        auto frame_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);
        if (frame_elapsed < target_frame_duration) {
            std::this_thread::sleep_for(target_frame_duration - frame_elapsed);
        }
    }

    video_capture.release();
    cv::destroyAllWindows();

    g_exit_server_thread.store(true);
    server_thread.join();
    
    g_exit_embedding_thread.store(true);
    g_embedding_buffer_cv.notify_one();
    embedding_thread.join();
    
    g_exit_detection_thread.store(true);
    detection_thread.join();
    
    g_should_record.store(false);
    g_exit_recording_thread.store(true);
    recording_thread.join();
    
    g_exit_db_thread.store(true);
    g_sql_queue_cv.notify_one();
    db_thread.join();
    
    g_exit_fps_thread.store(true);
    fps_thread.join();
    
    g_retinaface_net.clear();
    g_mobilefacenet_net.clear();

    return 0;
}

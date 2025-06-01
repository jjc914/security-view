#ifndef DETECTION_HPP
#define DETECTION_HPP

#include <opencv2/opencv.hpp>

#include <iostream>
#include <mutex>

#include "../globals.hpp"

void detection_thread_func() {
    std::cout << "[detect] info: starting face detection thread.\n";

    ncnn::Net scrfd_net;
    scrfd_net.opt.num_threads = 4;
    if (scrfd_net.load_param("models/scrfd/scrfd_500m-opt2.param") != 0 ||
            scrfd_net.load_model("models/scrfd/scrfd_500m-opt2.bin") != 0) {
        std::cerr << "[detect] error: failed to load SCRFD model." << std::endl;
        return;
    }
    std::cout << "[detect] info: SCRFD model loaded successfully.\n";

    while (!g_exit_detection_thread.load()) {
        cv::Mat frame;
        { std::lock_guard<std::mutex> lock(g_frame_mutex);
            if (g_frame.empty()) continue;
            frame = g_frame.clone();
        }
        // preprocessing
        cv::Mat resized;
        cv::resize(frame, resized, cv::Size(640, 640));

        ncnn::Mat in = ncnn::Mat::from_pixels(resized.data, ncnn::Mat::PIXEL_BGR, 640, 640);
        const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
        const float norm_vals[3] = {1 / 128.f, 1 / 128.f, 1 / 128.f};
        in.substract_mean_normalize(mean_vals, norm_vals);

        // inference        
        ncnn::Extractor ex = scrfd_net.create_extractor();
        ex.input("input.1", in);
        ncnn::Mat out;
        ex.extract("output0", out);
        for (int i = 0; i < out.h; ++i) {
            const float* values = out.row(i);
            float score = values[4];
            if (score < 0.5f) continue;
        
            float x0 = values[0] * frame.cols;
            float y0 = values[1] * frame.rows;
            float x1 = values[2] * frame.cols;
            float y1 = values[3] * frame.rows;
            std::cout << "[detect] info: found face with bounding box (" << x0 << ", " << y0 << "), (" << x1 << ", " << y1 << ").\n";
        }
    }

    std::cout << "[detect] info: exiting detection thread.\n";
}

#endif
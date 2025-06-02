#ifndef RECORDING_HPP
#define RECORDING_HPP

#include <opencv2/opencv.hpp>

#include <iostream>
#include <chrono>
#include <string>

#include "../globals.hpp"

void recording_thread_func(const cv::Size frame_size) {
    g_exit_recording_thread.store(false);
    std::cout << "[rec] info: starting recording thread.\n";

    // parameters
    const float activate_time = 1; // seconds
    const float deactivate_time = 2;
    const float prerecord_buffer = 2;

    const int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');

    bool is_first_found = false;
    std::chrono::time_point<std::chrono::steady_clock> first_found_time;
    while (!g_exit_recording_thread.load()) {
        const int fps = g_fps.load();
        if (fps < 1) continue;

        // remove expired frames
        const auto steady_now = std::chrono::steady_clock::now();
        { std::lock_guard<std::mutex> lock(g_recording_buffer_mutex);
            if (!g_recording_buffer.empty()) {
                while ((steady_now - g_recording_buffer.front().steady_time) >= std::chrono::duration<float>(prerecord_buffer)) {
                    g_recording_buffer.pop();
                }
            }
        }

        // check if should start recording
        if (g_should_record.load()) {
            if (!is_first_found) {
                // first head detected, start the timer
                std::cout << "[rec] info: detected head, starting timer" << std::endl;
                first_found_time = std::chrono::steady_clock::now();
                is_first_found = true;
            }

            // wait for detected for some period
            if ((steady_now - first_found_time) >= std::chrono::duration<float>(activate_time)) {
                // activate recording
                std::string now_str = current_date_time_str();
                std::cout << "[rec] info: starting recording at " << now_str << "\n";
                std::cout << "[rec] info: writing recording to " << "rec/" << now_str <<  ".avi\n";
                cv::VideoWriter video_writer = cv::VideoWriter("rec/" + now_str + ".avi", fourcc, fps, frame_size, true);
                if (!video_writer.isOpened()) {
                    std::cerr << "[rec] error: could not open video writer." << std::endl;
                }
                // keep recording until not
                std::chrono::time_point<std::chrono::steady_clock> last_found_time = first_found_time;
                while ((std::chrono::steady_clock::now() - last_found_time) <= std::chrono::duration<float>(deactivate_time)) {
                    if (g_should_record.load()) {
                        last_found_time = std::chrono::steady_clock::now();
                    }
                    // write all buffered data
                    { std::unique_lock<std::mutex> lock(g_recording_buffer_mutex, std::defer_lock);
                        while (!g_recording_buffer.empty()) {
                            lock.lock();
                            cv::Mat frame = g_recording_buffer.front().frame;
                            g_recording_buffer.pop();
                            lock.unlock();
                            video_writer.write(frame);
                        }
                    }
                }

                // end recording
                std::cout << "[rec] info: ending recording at " << current_date_time_str() << "\n";
                video_writer.release();
            }
        } else {
            is_first_found = false;
        }
    }
    std::cout << "[rec] info: exiting recording thread.\n";
}

#endif
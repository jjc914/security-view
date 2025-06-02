#ifndef SERVER_HPP
#define SERVER_HPP

#include "../utils.hpp"

#include <opencv2/opencv.hpp>
#include <httplib.h>

#include <iostream>
#include <chrono>
#include <cstdint>
#include <thread>
#include <mutex>
#include <atomic>

#include "../globals.hpp"

void server_thread_func(void) {
    g_exit_server_thread.store(false);
    std::cout << "[server] info: starting server listening thread.\n";

    std::string ip = "0.0.0.0";
    uint16_t port = 8080;
    httplib::Server server;

    server.Get("/video_raw", [&](const httplib::Request& req, httplib::Response& res) {
        // set the mime type to indicate multipart mjpeg stream
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");

        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&](size_t offset, httplib::DataSink &sink) -> bool {
                // write data
                std::vector<uchar> buf;
                std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };

                while (sink.is_writable()) {
                    if (g_exit_server_thread.load()) break;
                    // if (!g_should_stream.load()) continue;
                    
                    cv::Mat frame;
                    { std::lock_guard<std::mutex> lock(g_streaming_buffer_mutex);
                        if (g_streaming_buffer.empty()) continue;
                        frame = g_streaming_buffer.clone();
                    }
                    // Encode the captured frame as JPEG.
                    cv::imencode(".jpg", frame, buf, params);

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

    server.Get("/video_retina", [&](const httplib::Request& req, httplib::Response& res) {
        // set the mime type to indicate multipart mjpeg stream
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");

        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&](size_t offset, httplib::DataSink &sink) -> bool {
                // write data
                std::vector<uchar> buf;
                std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };

                while (sink.is_writable()) {
                    if (g_exit_server_thread.load()) break;
                    // if (!g_should_stream.load()) continue;
                    
                    cv::Mat frame;
                    { std::lock_guard<std::mutex> lock(g_retina_debug_buffer_mutex);
                        if (g_retina_debug_buffer.empty()) continue;
                        frame = g_retina_debug_buffer.clone();
                    }
                    // Encode the captured frame as JPEG.
                    cv::imencode(".jpg", frame, buf, params);

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
        res.set_content(read_file("web/index.html"), "text/html");
    });

    server.Post("/togglestream", [](const httplib::Request& req, httplib::Response& res) {
        g_should_stream.store(!g_should_stream.load());
    });

    int bind_result = server.bind_to_port(ip, port);
    if (bind_result <= 0) {
        std::cerr << "[server] error: failed to bind to port " << port << "\n";
        return;
    }

    std::cout << "[server] info: server bound on http://" << ip << ":" << port << "\n";
    
    std::thread server_thread([&]() {
        server.listen_after_bind();
    });

    while (!g_exit_server_thread.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "[server] info: exiting server thread.\n";

    server.stop();
    server_thread.join();
}

#endif
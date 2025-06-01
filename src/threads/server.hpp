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
                    if (!g_should_stream.load()) continue;
                    
                    cv::Mat frame;
                    { std::lock_guard<std::mutex> lock(g_frame_mutex);
                        if (g_frame.empty()) continue;
                        frame = g_frame.clone();
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

    std::cout << "[server] info: server is running on http://"<< ip << ":" << port << "." << std::endl;
    std::thread listen_thread([&server]() {
        server.listen("0.0.0.0", 8080);
    });

    while (!g_exit_server_thread.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "[server] info: exiting server thread.\n";
    server.stop();
    listen_thread.join();
}

#endif
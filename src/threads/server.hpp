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

    std::string IP = "0.0.0.0";
    uint16_t PORT = 8080;
    httplib::Server server;

    server.Get("/video_raw", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");

        cv::Mat fallback_img = cv::imread("web/dc.png");
        std::vector<uchar> fallback_buf;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };
        if (!fallback_img.empty()) {
            cv::imencode(".jpg", fallback_img, fallback_buf, params);
        } else {
            std::cerr << "Warning: could not load fallback image from res/dc.png\n";
        }

        if (fallback_buf.empty()) {
            std::cerr << "[server] error: fallback image encoding failed or file was unreadable.\n";
            res.status = 500;
            res.set_content("Fallback image not loaded or invalid.", "text/plain");
            return;
        }

        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&](size_t offset, httplib::DataSink &sink) -> bool {
                // write data
                std::vector<uchar> buf;
                std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };

                while (sink.is_writable()) {
                    if (g_exit_server_thread.load()) break;
                    
                    cv::Mat frame;
                    { std::lock_guard<std::mutex> lock(g_streaming_buffer_mutex);
                        if (!g_streaming_buffer.empty())
                            frame = g_streaming_buffer.clone();
                    }

                    if (frame.empty()) {
                        buf = fallback_buf;
                    } else {
                        bool ok = cv::imencode(".jpg", frame, buf, params);
                        if (!ok || buf.empty()) {
                            std::cerr << "[server] warning: frame encoding failed, falling back.\n";
                            buf = fallback_buf;
                        }
                    }

                    std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                        std::to_string(buf.size()) + "\r\n\r\n";
                    if (!sink.write(header.c_str(), header.size())) // header
                        break;
                    if (!sink.write(reinterpret_cast<const char*>(buf.data()), buf.size())) // jpeg data
                        break;
                    if (!sink.write("\r\n", 2)) // trailing newline
                        break;
                }
                return true;
            }
        );
    });

    server.Get("/video_retina_output", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");

        cv::Mat fallback_img = cv::imread("web/dc.png");
        std::vector<uchar> fallback_buf;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };
        if (!fallback_img.empty()) {
            cv::imencode(".jpg", fallback_img, fallback_buf, params);
        } else {
            std::cerr << "Warning: could not load fallback image from res/dc.png\n";
        }

        if (fallback_buf.empty()) {
            std::cerr << "[server] error: fallback image encoding failed or file was unreadable.\n";
            res.status = 500;
            res.set_content("Fallback image not loaded or invalid.", "text/plain");
            return;
        }

        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&](size_t offset, httplib::DataSink &sink) -> bool {
                // write data
                std::vector<uchar> buf;
                std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };

                while (sink.is_writable()) {
                    if (g_exit_server_thread.load()) break;
                    
                    cv::Mat frame;
                    { std::lock_guard<std::mutex> lock(g_retina_debug_buffer_1_mutex);
                        if (!g_retina_debug_buffer_1.empty())
                            frame = g_retina_debug_buffer_1.clone();
                    }

                    if (frame.empty()) {
                        buf = fallback_buf;
                    } else {
                        bool ok = cv::imencode(".jpg", frame, buf, params);
                        if (!ok || buf.empty()) {
                            std::cerr << "[server] warning: frame encoding failed, falling back.\n";
                            buf = fallback_buf;
                        }
                    }

                    std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                        std::to_string(buf.size()) + "\r\n\r\n";
                    if (!sink.write(header.c_str(), header.size())) // header
                        break;
                    if (!sink.write(reinterpret_cast<const char*>(buf.data()), buf.size())) // jpeg data
                        break;
                    if (!sink.write("\r\n", 2)) // trailing newline
                        break;
                }
                return true;
            }
        );
    });

    server.Get("/video_retina_input", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");

        cv::Mat fallback_img = cv::imread("web/dc.png");
        std::vector<uchar> fallback_buf;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };
        if (!fallback_img.empty()) {
            cv::imencode(".jpg", fallback_img, fallback_buf, params);
        } else {
            std::cerr << "Warning: could not load fallback image from res/dc.png\n";
        }

        if (fallback_buf.empty()) {
            std::cerr << "[server] error: fallback image encoding failed or file was unreadable.\n";
            res.status = 500;
            res.set_content("Fallback image not loaded or invalid.", "text/plain");
            return;
        }

        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&](size_t offset, httplib::DataSink &sink) -> bool {
                // write data
                std::vector<uchar> buf;
                std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };

                while (sink.is_writable()) {
                    if (g_exit_server_thread.load()) break;
                    
                    cv::Mat frame;
                    { std::lock_guard<std::mutex> lock(g_retina_debug_buffer_2_mutex);
                        if (!g_retina_debug_buffer_2.empty())
                            frame = g_retina_debug_buffer_2.clone();
                    }

                    if (frame.empty()) {
                        buf = fallback_buf;
                    } else {
                        bool ok = cv::imencode(".jpg", frame, buf, params);
                        if (!ok || buf.empty()) {
                            std::cerr << "[server] warning: frame encoding failed, falling back.\n";
                            buf = fallback_buf;
                        }
                    }

                    std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                        std::to_string(buf.size()) + "\r\n\r\n";
                    if (!sink.write(header.c_str(), header.size())) // header
                        break;
                    if (!sink.write(reinterpret_cast<const char*>(buf.data()), buf.size())) // jpeg data
                        break;
                    if (!sink.write("\r\n", 2)) // trailing newline
                        break;
                }
                return true;
            }
        );
    });

    server.Get("/video_annotated", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");

        cv::Mat fallback_img = cv::imread("web/dc.png");
        std::vector<uchar> fallback_buf;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };
        if (!fallback_img.empty()) {
            cv::imencode(".jpg", fallback_img, fallback_buf, params);
        } else {
            std::cerr << "Warning: could not load fallback image from res/dc.png\n";
        }

        if (fallback_buf.empty()) {
            std::cerr << "[server] error: fallback image encoding failed or file was unreadable.\n";
            res.status = 500;
            res.set_content("Fallback image not loaded or invalid.", "text/plain");
            return;
        }

        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&](size_t offset, httplib::DataSink &sink) -> bool {
                // write data
                std::vector<uchar> buf;
                std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };

                while (sink.is_writable()) {
                    if (g_exit_server_thread.load()) break;
                    
                    cv::Mat frame;
                    { std::lock_guard<std::mutex> lock(g_annotated_streaming_buffer_mutex);
                        if (!g_annotated_streaming_buffer.empty())
                            frame = g_annotated_streaming_buffer.clone();
                    }

                    if (frame.empty()) {
                        buf = fallback_buf;
                    } else {
                        bool ok = cv::imencode(".jpg", frame, buf, params);
                        if (!ok || buf.empty()) {
                            std::cerr << "[server] warning: frame encoding failed, falling back.\n";
                            buf = fallback_buf;
                        }
                    }

                    std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                        std::to_string(buf.size()) + "\r\n\r\n";
                    if (!sink.write(header.c_str(), header.size())) // header
                        break;
                    if (!sink.write(reinterpret_cast<const char*>(buf.data()), buf.size())) // jpeg data
                        break;
                    if (!sink.write("\r\n", 2)) // trailing newline
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

    int bind_result = server.bind_to_port(IP, PORT);
    if (bind_result <= 0) {
        std::cerr << "[server] error: failed to bind to port " << PORT << "\n";
        return;
    }

    std::cout << "[server] info: server bound on http://" << IP << ":" << PORT << "\n";
    
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
#ifndef SERVER_HPP
#define SERVER_HPP

#include "../utils.hpp"

#include <opencv2/opencv.hpp>
#include <httplib.h>
#include <httplib_ssl.h>

#include <iostream>
#include <chrono>
#include <cstdint>
#include <thread>
#include <mutex>
#include <atomic>

#include "../globals.hpp"

namespace {

// chatgpt generate session token function
std::string generate_session_token() {
    std::random_device rd;
    std::mt19937_64 gen(rd()); // 64-bit Mersenne Twister
    std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream oss;
    for (int i = 0; i < 4; ++i) { // 4 * 64 bits = 256 bits (~32-character token)
        uint64_t part = dis(gen);
        oss << std::hex << std::setw(16) << std::setfill('0') << part;
    }
    return oss.str();
}

bool is_authenticated(const httplib::Request& req) {
    auto it = req.headers.find("Cookie");
    if (it != req.headers.end()) {
        std::string cookie = it->second;
        size_t pos = cookie.find("session=");
        if (pos != std::string::npos) {
            std::string token = cookie.substr(pos + 8);
            size_t semicolon = token.find(';');
            if (semicolon != std::string::npos)
                token = token.substr(0, semicolon);
            
            { std::lock_guard<std::mutex> lock(g_valid_sessions_mutex);
                auto iter = g_valid_sessions.find(token);
                if (iter != g_valid_sessions.end()) {
                    auto now = std::chrono::steady_clock::now();
                    if (now < iter->second) {
                        // session is valid
                        return true;
                    } else {
                        // session is expired
                        g_valid_sessions.erase(iter);
                    }
                }
            }
        }
    }
    return false;
}

template <typename Handler>
void with_auth(const httplib::Request& req, httplib::Response& res, Handler handler, bool redirect_on_fail = true) {
    if (!is_authenticated(req)) {
        if (redirect_on_fail) {
            res.set_redirect("/login");
        } else {
            res.status = 403;
            res.set_content("Forbidden", "text/plain");
        }
        return;
    }
    handler();
}

}

void server_thread_func(void) {
    g_exit_server_thread.store(false);
    std::cout << "[server] info: starting server listening thread.\n";

    const std::string ADMIN_USERNAME = "admin";
    const std::string ADMIN_PASSWORD = "admin";

    const std::string IP = "0.0.0.0";
    const uint16_t PORT = 8080;
    httplib::SSLServer server("certs/cert.pem", "certs/key.pem");

    server.Get("/", [&](const httplib::Request &req, httplib::Response &res) {
        with_auth(req, res, [&]() {
            res.set_content(read_file("web/index.html"), "text/html");
        });
    });

    server.Get("/login", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(read_file("web/login.html"), "text/html");
    });

    server.Post("/login", [&](const httplib::Request& req, httplib::Response& res) {
        auto usr_it = req.params.find("usr");
        auto pwd_it = req.params.find("pwd");
        if (usr_it != req.params.end()) {
            if (pwd_it != req.params.end()) {
                const std::string& usr = usr_it->second;
                const std::string& pwd = pwd_it->second;
                if (usr == ADMIN_USERNAME && pwd == ADMIN_PASSWORD) {
                    // successful login
                    std::string token = generate_session_token();
                    std::chrono::minutes session_duration(30); // 30 min sessions
                    { std::lock_guard<std::mutex> lock(g_valid_sessions_mutex);
                        g_valid_sessions[token] = std::chrono::steady_clock::now() + session_duration;
                    }

                    // set session cookie and redirect
                    res.set_header("Set-Cookie", "session=" + token + "; HttpOnly; Path=/");
                    res.set_redirect("/"); // redirect to main page
                    return;
                }
            }
        }

        // login failed
        res.status = 401;
        res.set_content("Invalid password", "text/plain");
    });


    server.Post("/togglestream", [](const httplib::Request& req, httplib::Response& res) {
        g_should_stream.store(!g_should_stream.load());
    });

    server.Get("/video_raw", [&](const httplib::Request& req, httplib::Response& res) {
        with_auth(req, res, [&]() {
            res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");
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
                            if (!g_streaming_buffer.empty()) {
                                frame = g_streaming_buffer.clone();
                            } else {
                                frame.release();
                            }
                        }
                        if (frame.empty()) {
                            continue;
                        }
                        
                        try {
                            bool ok = cv::imencode(".jpg", frame, buf, params);
                            if (!ok || buf.empty()) {
                                std::cerr << "[server] warning: frame encoding failed, skipping frame.\n";
                                continue; // skip this frame
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "[server] exception during imencode: " << e.what() << "\n";
                            continue;
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
        }, false);
    });

    server.Get("/video_retina_output", [&](const httplib::Request& req, httplib::Response& res) {
        with_auth(req, res, [&]() {
            res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");
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
                            continue;
                        }
                        
                        try {
                            bool ok = cv::imencode(".jpg", frame, buf, params);
                            if (!ok || buf.empty()) {
                                std::cerr << "[server] warning: frame encoding failed, skipping frame.\n";
                                continue; // skip this frame
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "[server] exception during imencode: " << e.what() << "\n";
                            continue;
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
        }, false);
    });

    server.Get("/video_retina_input", [&](const httplib::Request& req, httplib::Response& res) {
        with_auth(req, res, [&]() {
            res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");
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
                            continue;
                        }
                        
                        try {
                            bool ok = cv::imencode(".jpg", frame, buf, params);
                            if (!ok || buf.empty()) {
                                std::cerr << "[server] warning: frame encoding failed, skipping frame.\n";
                                continue; // skip this frame
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "[server] exception during imencode: " << e.what() << "\n";
                            continue;
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
        }, false);
    });

    server.Get("/video_annotated", [&](const httplib::Request& req, httplib::Response& res) {
        with_auth(req, res, [&]() {
            res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");
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
                            return true;
                        }

                        try {
                            bool ok = cv::imencode(".jpg", frame, buf, params);
                            if (!ok || buf.empty()) {
                                std::cerr << "[server] warning: frame encoding failed, skipping frame.\n";
                                continue; // skip this frame
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "[server] exception during imencode: " << e.what() << "\n";
                            continue;
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
        }, false);
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
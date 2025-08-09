#ifndef SERVER_HPP
#define SERVER_HPP

#include <opencv2/opencv.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../utils.hpp"
#include "detection.hpp"
#include "embedding.hpp"

#include <iostream>
#include <chrono>
#include <cstdint>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <unordered_set>

#include "../globals.hpp"

using json = nlohmann::json;

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
    const uint16_t PORT = 8443;
    httplib::SSLServer server("certs/cert.pem", "certs/key.pem");

    // page endpoints
    server.Get("/login", [&](const httplib::Request& req, httplib::Response& res) {
        if (is_authenticated(req)) {
            res.set_redirect("/"); // redirect to main page
            return;
        };

        res.set_content(read_file("web/login.html"), "text/html");
    });
    
    server.Get("/", [&](const httplib::Request &req, httplib::Response &res) {
        with_auth(req, res, [&]() {
            res.set_content(read_file("web/index.html"), "text/html");
        });
    });

    server.Get("/admin", [&](const httplib::Request& req, httplib::Response& res) {
        with_auth(req, res, [&]() {
            res.set_content(read_file("web/admin.html"), "text/html");
        });
    });

    // get endpoints
    server.Get("/get_faces", [&](const httplib::Request& req, httplib::Response& res) {
        with_auth(req, res, [&]() {
            SQLQuery query;
            query.priority = SQLQuery::Priority::LOW;
            query.sql = R"SQL(
                SELECT DISTINCT name FROM people ORDER BY name COLLATE NOCASE ASC;
            )SQL";

            std::vector<std::string> face_names;

            query.on_row = [&face_names](sqlite3_stmt* stmt) {
                const char* name_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                std::string name = name_text ? name_text : "";
                face_names.push_back(std::move(name));
            };

            bool is_done = false;
            std::mutex is_done_mutex;
            std::condition_variable is_done_cv;

            query.on_done = [&]() {
                std::unique_lock<std::mutex> lock(is_done_mutex);
                is_done = true;
                is_done_cv.notify_one();
            };

            { std::lock_guard<std::mutex> lock(g_sql_queue_mutex);
                g_sql_queue.push(std::move(query));
            }
            g_sql_queue_cv.notify_one();

            { std::unique_lock<std::mutex> lock(is_done_mutex);
                is_done_cv.wait(lock, [&]() { return is_done; });
            }

            json j;
            j["faces"] = face_names;

            res.set_content(j.dump(), "application/json");
        }, false);
    });

    // post endpoints
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
                    res.set_header("Set-Cookie", "session=" + token + "; HttpOnly; Path=/; Secure");
                    res.set_redirect("/"); // redirect to main page
                    return;
                }
            }
        }

        // login failed
        res.status = 401;
        res.set_content("Invalid password", "text/plain");
    });
    
    server.Post("/detect_faces", [&](const httplib::Request& req, httplib::Response& res) {
        with_auth(req, res, [&]() {
            std::cout << "[server] info: received detect_faces request.\n";
            auto file_it = req.files.find("imageFile");

            if (file_it == req.files.end()) {
                res.status = 400;
                res.set_content("Missing image", "text/plain");
                return;
            }

            const auto& file = file_it->second;
            std::vector<uchar> data(file.content.begin(), file.content.end());
            cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);

            if (img.empty()) {
                res.status = 400;
                res.set_content("Invalid image", "text/plain");
                return;
            }

            auto db_faces = load_embedding_database();

            std::vector<FaceObject> detected_faces = detect_faces(img);

            json j_response;
            j_response["boxes"] = json::array();

            for (size_t i = 0; i < detected_faces.size(); ++i) {
                const auto& fo = detected_faces[i];
                const auto& r = fo.rect;

                cv::Mat transform = cv::estimateAffinePartial2D(fo.landmarks, g_EMBEDDING_REFERENCE);
                cv::Mat aligned;
                cv::warpAffine(img, aligned, transform, cv::Size(112, 112), cv::INTER_LINEAR);

                std::vector<float> embedding = compute_feature_embedding(aligned);

                std::string best_guess;
                float best_sim = -1.0f;
                for (const auto& db_face : db_faces) {
                    float dot_product = dot(embedding, db_face.embedding);

                    if (dot_product > best_sim) {
                        best_sim = dot_product;
                        best_guess = db_face.name;
                    }
                }

                const float threshold = 0.8f;
                std::string guess_name = (best_sim > threshold) ? best_guess : "";

                json j_box;
                j_box["face_index"] = i;
                j_box["x"] = r.x;
                j_box["y"] = r.y;
                j_box["width"] = r.width;
                j_box["height"] = r.height;
                if (guess_name.empty()) {
                    j_box["guess"] = nullptr;
                } else {
                    j_box["guess"] = guess_name;
                }

                j_response["boxes"].push_back(j_box);
            }

            res.set_content(j_response.dump(), "application/json");
        }, false);
    });

    server.Post("/register_faces", [&](const httplib::Request& req, httplib::Response& res) {
        with_auth(req, res, [&]() {
            std::cout << "[server] info: received register_faces request.\n";

            auto faces_json_it = req.files.find("facesJSON");
            if (faces_json_it == req.files.end()) {
                res.status = 400;
                res.set_content("Missing facesJSON", "text/plain");
                return;
            }

            std::vector<std::pair<size_t, std::string>> faces_to_register;
            try {
                auto parsed = json::parse(faces_json_it->second.content);

                for (const auto& face_entry : parsed) {
                    size_t face_index = face_entry.at("face_index").get<size_t>();
                    std::string name = face_entry.at("name").get<std::string>();
                    faces_to_register.emplace_back(face_index, name);
                }
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("Invalid faces JSON: ") + e.what(), "text/plain");
                return;
            }

            auto file_it = req.files.find("imageFile");
            if (file_it == req.files.end()) {
                res.status = 400;
                res.set_content("Missing image", "text/plain");
                return;
            }

            const auto& file = file_it->second;
            std::vector<uchar> data(file.content.begin(), file.content.end());
            cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);

            if (img.empty()) {
                res.status = 400;
                res.set_content("Invalid image", "text/plain");
                return;
            }

            std::vector<FaceObject> detected_faces = detect_faces(img);

            for (const auto& [face_index, name] : faces_to_register) {
                if (face_index >= detected_faces.size()) {
                    res.status = 400;
                    res.set_content("Invalid face index", "text/plain");
                    return;
                }
            }

            for (const auto& [face_index, name] : faces_to_register) {
                FaceObject& fo = detected_faces[face_index];

                cv::Mat transform = cv::estimateAffinePartial2D(fo.landmarks, g_EMBEDDING_REFERENCE);
                cv::Mat aligned;
                cv::warpAffine(img, aligned, transform, cv::Size(112, 112), cv::INTER_LINEAR);

                std::vector<float> embedding = compute_feature_embedding(aligned);

                SQLQuery insert_person_query;
                insert_person_query.priority = SQLQuery::Priority::HIGH;
                insert_person_query.sql = R"SQL(INSERT OR IGNORE INTO people (name) VALUES (?);)SQL";
                insert_person_query.on_bind = [name](sqlite3_stmt* stmt) {
                    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
                };

                SQLQuery insert_embedding_query;
                insert_embedding_query.priority = SQLQuery::Priority::HIGH;
                insert_embedding_query.sql = R"SQL(
                    INSERT INTO embeddings (person_id, vec, img_src)
                    VALUES ((SELECT person_id FROM people WHERE name=?), ?, '')
                )SQL";
                insert_embedding_query.on_bind = [name, embedding = std::move(embedding)](sqlite3_stmt* stmt) {
                    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_blob(stmt, 2, embedding.data(), embedding.size() * sizeof(float), SQLITE_TRANSIENT);
                };

                {
                    std::lock_guard<std::mutex> lock(g_sql_queue_mutex);
                    g_sql_queue.push(std::move(insert_person_query));
                    g_sql_queue.push(std::move(insert_embedding_query));
                }
                g_sql_queue_cv.notify_one();

                std::cout << "[server] info: registered face_index=" << face_index << ", name='" << name << "'.\n";
            }

            g_should_reload_db.store(true);
            res.set_content("Faces registered successfully", "text/plain");
        }, false);
    });

    // stream endpoints
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

    // web elements
    server.Get(R"(/(css/.*|js/.*|pages/.*))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string path = "web" + req.path;
        std::string content_type = "text/plain";

        if (ends_with(path, ".html")) content_type = "text/html";
        if (ends_with(path, ".css")) content_type = "text/css";
        else if (ends_with(path, ".js")) content_type = "application/javascript";
        else if (ends_with(path, ".png")) content_type = "image/png";
        else if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) content_type = "image/jpeg";

        res.set_content(read_file(path), content_type);
    });

    server.Get("/shutdown", [&](const httplib::Request& req, httplib::Response& res) {
        g_exit_main_thread.store(true);
        res.set_content("Shutting down...", "text/plain");
    });

    int bind_result = server.bind_to_port(IP, PORT);
    if (bind_result <= 0) {
        std::cerr << "[server] error: failed to bind to port " << PORT << "\n";
        return;
    }

    std::cout << "[server] info: server bound on https://" << IP << ":" << PORT << "\n";
    
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
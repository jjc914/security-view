#ifndef EMBEDDING_HPP
#define EMBEDDING_HPP

#include <opencv2/opencv.hpp>

#include <net.h>
#include <mat.h>
#include <layer.h>

#include "../types.hpp"
#include "../utils.hpp"

#include <iostream>
#include <mutex>
#include <array>
#include <condition_variable>

#include "../globals.hpp"

std::vector<EmbeddingEntry> load_embedding_database(void) {
    std::vector<EmbeddingEntry> face_db;

    bool is_done = false;
    std::mutex is_done_mutex;
    std::condition_variable is_done_cv;
    SQLQuery db_query;
    db_query.sql = R"SQL(
        SELECT people.name, embeddings.vec
        FROM embeddings
        JOIN people ON embeddings.person_id = people.person_id
        )SQL";
    db_query.on_row = [&face_db](sqlite3_stmt* stmt) {
        // read name
        const char* name_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string name = name_text ? name_text : "";

        // read embedding blob
        const void* blobData = sqlite3_column_blob(stmt, 1);
        int blobSize = sqlite3_column_bytes(stmt, 1);
        size_t numFloats = blobSize / sizeof(float);

        std::vector<float> embedding(numFloats);
        memcpy(embedding.data(), blobData, blobSize);

        // normalize
        float norm = 0.f;
        for (float val : embedding) norm += val * val;
        norm = std::sqrt(norm);
        for (float& val : embedding) val /= norm;

        // add to face_db
        EmbeddingEntry entry;
        entry.name = std::move(name);
        entry.embedding = std::move(embedding);
        face_db.push_back(std::move(entry));
    };
    db_query.on_done = [&is_done, &is_done_mutex, &is_done_cv]() {
        { std::unique_lock<std::mutex> lock(is_done_mutex);
            is_done = true;
        }
        is_done_cv.notify_one();
    };
    db_query.priority = SQLQuery::Priority::HIGH;
    g_sql_queue.push(std::move(db_query));
    g_sql_queue_cv.notify_one();

    { std::unique_lock<std::mutex> lock(is_done_mutex);
        is_done_cv.wait(lock, [&is_done] { return is_done; });
    }

    std::cout << "[embed] info: loaded database.\n";
    return face_db;
}

std::vector<float> compute_feature_embedding(const cv::Mat& face) {
    ncnn::Extractor ex = g_mobilefacenet_net.create_extractor();
    
    ncnn::Mat in = ncnn::Mat::from_pixels(face.data, ncnn::Mat::PIXEL_BGR2RGB, 112, 112);
    
    ex.set_light_mode(true);
    ex.input("data", in);

    ncnn::Mat feat;
    if (ex.extract("fc1", feat) != 0) {
        std::cerr << "[embed] error: failed to extract feature embedding." << std::endl;
        return std::move(std::vector<float>());
    }

    std::vector<float> embedding(feat.w);
    float norm = 0.f;
    for (int i = 0; i < feat.w; i++) {
        embedding[i] = feat[i];
        norm += embedding[i] * embedding[i];
    }
    norm = std::sqrt(norm);
    for (int i = 0; i < feat.w; i++) {
        embedding[i] /= norm;
    }

    return std::move(embedding);
}

void embedding_thread_func(void) {
    g_exit_embedding_thread.store(false);
    std::cout << "[embed] info: starting facial feature embedding thread.\n";

    // load embedding db
    std::vector<EmbeddingEntry> face_db = load_embedding_database();

    while (!g_exit_embedding_thread.load()) {
        DetectionResult retina;
        { std::unique_lock<std::mutex> lock(g_embedding_buffer_mutex);
            g_embedding_buffer_cv.wait(lock, [] {
                return !g_embedding_buffer.empty() || g_should_reload_db.load() || g_exit_embedding_thread.load();
            });

            if (g_exit_embedding_thread.load()) break;

            if (g_should_reload_db.load()) {
                lock.unlock();
                face_db = load_embedding_database();
                g_should_reload_db.store(false);
                continue;
            }

            retina = g_embedding_buffer.front();
            g_embedding_buffer.pop();
        }

        cv::Mat annotated = retina.frame.clone();
        for (FaceObject& fo : retina.faces) {
            // align face
            cv::Mat transform = cv::estimateAffinePartial2D(fo.landmarks, g_EMBEDDING_REFERENCE);
            cv::Mat aligned;
            cv::warpAffine(retina.frame, aligned, transform, cv::Size(112, 112), cv::INTER_LINEAR);

            if (aligned.cols != 112 || aligned.rows != 112)
                cv::resize(aligned, aligned, cv::Size(112, 112));

            std::vector<float> embedding = compute_feature_embedding(aligned);

            int match_count = 0;
            for (size_t i = 0; i < face_db.size(); ++i) {
                if (dot(embedding, face_db[i].embedding) > 0.7f) {
                    // match
                    ++match_count;
                }
            }
            cv::rectangle(annotated, fo.rect, cv::Scalar(0, 255, 0), 2);
        }

        { std::lock_guard<std::mutex> lock(g_annotated_streaming_buffer_mutex);
            g_annotated_streaming_buffer = std::move(annotated);
        }
    }

    std::cout << "[embed] info: exiting facial feature embedding thread.\n";
}

#endif

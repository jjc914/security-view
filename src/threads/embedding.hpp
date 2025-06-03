#ifndef EMBEDDING_HPP
#define EMBEDDING_HPP

#include <opencv2/opencv.hpp>

#include <net.h>
#include <mat.h>
#include <layer.h>

#include "../types.hpp"

#include <iostream>
#include <mutex>
#include <array>

#include "../globals.hpp"

namespace {

std::vector<float> compute_feature_embedding(ncnn::Net& mobilefacenet, const cv::Mat& face) {
    ncnn::Mat in = ncnn::Mat::from_pixels(face.data, ncnn::Mat::PIXEL_BGR2RGB, 112, 112);
    const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
    const float norm_vals[3] = {1.f / 128.f, 1.f / 128.f, 1.f / 128.f};
    in.substract_mean_normalize(mean_vals, norm_vals);
    
    ncnn::Extractor ex = mobilefacenet.create_extractor();
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

std::vector<EmbeddingEntry> load_database_binary(const std::string& path) {
    std::vector<EmbeddingEntry> database;
    std::ifstream in(path, std::ios::binary);

    if (!in) {
        std::cerr << "Error: could not open file for reading: " << path << "\n";
        return database;
    }

    while (in.peek() != EOF) {
        EmbeddingEntry entry;
        uint32_t name_len = 0, embed_len = 0;

        // Read name length and name
        in.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        if (in.eof()) break;

        entry.name.resize(name_len);
        in.read(&entry.name[0], name_len);

        // Read embedding length and values
        in.read(reinterpret_cast<char*>(&embed_len), sizeof(embed_len));
        entry.embedding.resize(embed_len);
        in.read(reinterpret_cast<char*>(entry.embedding.data()), embed_len * sizeof(float));

        // Add to DB
        database.push_back(std::move(entry));
    }

    in.close();
    return database;
}

float dot(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        std::cerr << "error: dot product vector sizes do not match.\n";
        return 0;
    }

    float s = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        s += a[i] * b[i];
    }
    return s;
}

}

void embedding_thread_func(void) {
    g_exit_embedding_thread.store(false);
    std::cout << "[embed] info: starting facial feature embedding thread.\n";

    ncnn::Net mobilefacenet_net;
    std::cout << "[detect] info: found " << ncnn::get_gpu_count() << " gpus.\n" << std::endl;
    mobilefacenet_net.opt.use_vulkan_compute = true;
    mobilefacenet_net.opt.num_threads = 4;
    // https://github.com/liguiyuan/mobilefacenet-ncnn/tree/master/models
    if (mobilefacenet_net.load_param("models/mobilefacenet/mobilefacenet.param") != 0 ||
            mobilefacenet_net.load_model("models/mobilefacenet/mobilefacenet.bin") != 0) {
        std::cerr << "[detect] error: failed to load MobileFaceNet model." << std::endl;
        return;
    }
    std::cout << "[detect] info: MobileFaceNet model loaded successfully.\n";

    // load embedding db
    std::vector<EmbeddingEntry> face_db = load_database_binary("res/face_db.bin");

    while (!g_exit_embedding_thread.load()) {
        cv::Mat face;
        { std::lock_guard<std::mutex> lock(g_embedding_buffer_mutex);
            if (g_embedding_buffer.empty()) continue;
            face = g_embedding_buffer.front();
            g_embedding_buffer.pop();
        }

        // preprocess frame
        if (face.cols != 112 || face.rows != 112)
            cv::resize(face, face, cv::Size(112, 112));

        std::vector<float> embedding = compute_feature_embedding(mobilefacenet_net, face);
        for (int i = 0; i < face_db.size(); ++i) {
            if (dot(embedding, face_db[i].embedding) > 0.5f) {
                // match
                std::cout << "recognized " << face_db[i].name << "!" << std::endl;
            }
        }
    }

    std::cout << "[embed] info: exiting facial feature embedding thread.\n";
}

#endif
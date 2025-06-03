#include <opencv2/opencv.hpp>

#include <net.h>
#include <mat.h>
#include <layer.h>

#include "../src/types.hpp"

#include <iostream>
#include <string>
#include <mutex>
#include <array>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

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

}

int main(int argc, char** argv) {
    ncnn::Net mobilefacenet_net;
    mobilefacenet_net.opt.use_vulkan_compute = true;
    mobilefacenet_net.opt.num_threads = 4;
    // https://github.com/liguiyuan/mobilefacenet-ncnn/tree/master/models
    if (mobilefacenet_net.load_param("models/mobilefacenet/mobilefacenet.param") != 0 ||
            mobilefacenet_net.load_model("models/mobilefacenet/mobilefacenet.bin") != 0) {
        std::cerr << "error: failed to load MobileFaceNet model." << std::endl;
        return 0;
    }
    std::cout << "info: RetinaFace model loaded successfully.\n";

    fs::path input_dir("res/recognized_faces");
    std::string output_path = "res/face_db.bin";
    std::string person_name = "joshua";  // hardcoded name for now

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "Error: could not open output file for writing: " << output_path << "\n";
        return 1;
    }

    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) continue;

        cv::Mat face = cv::imread(entry.path().string());
        if (face.empty()) {
            std::cerr << "Failed to read image: " << entry.path() << "\n";
            continue;
        }

        cv::cvtColor(face, face, cv::COLOR_BGR2RGB);

        std::vector<float> embedding = compute_feature_embedding(mobilefacenet_net, face);

        uint32_t name_len = person_name.size();
        uint32_t embed_len = embedding.size();

        out.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        out.write(person_name.data(), name_len);
        out.write(reinterpret_cast<const char*>(&embed_len), sizeof(embed_len));
        out.write(reinterpret_cast<const char*>(embedding.data()), embed_len * sizeof(float));

        std::cout << "Stored embedding for: " << entry.path() << "\n";
    }

    out.close();
    std::cout << "Finished writing to " << output_path << "\n";
    return 0;
}
#ifndef RETINA_HPP
#define RETINA_HPP

#include <opencv2/opencv.hpp>

#include <net.h>
#include <mat.h>
#include <layer.h>

#include "../types.hpp"

#include <iostream>
#include <string>
#include <mutex>
#include <array>

#include "../globals.hpp"

namespace { // https://github.com/Tencent/ncnn/blob/master/examples/retinaface.cpp

static inline float intersection_area(const FaceObject& a, const FaceObject& b) {
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}

static void qsort_descent_inplace(std::vector<FaceObject>& faceobjects, int left, int right) {
    int i = left;
    int j = right;
    float p = faceobjects[(left + right) / 2].prob;

    while (i <= j) {
        while (faceobjects[i].prob > p) ++i;
        while (faceobjects[j].prob < p) --j;

        if (i <= j) {
            // swap
            std::swap(faceobjects[i], faceobjects[j]);

            ++i;
            --j;
        }
    }

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            if (left < j) qsort_descent_inplace(faceobjects, left, j);
        }
        #pragma omp section
        {
            if (i < right) qsort_descent_inplace(faceobjects, i, right);
        }
    }
}

static void qsort_descent_inplace(std::vector<FaceObject>& faceobjects) {
    if (faceobjects.empty()) return;
    qsort_descent_inplace(faceobjects, 0, faceobjects.size() - 1);
}

static ncnn::Mat generate_anchors(int base_size, const ncnn::Mat& ratios, const ncnn::Mat& scales) {
    int num_ratio = ratios.w;
    int num_scale = scales.w;

    ncnn::Mat anchors;
    anchors.create(4, num_ratio * num_scale);

    const float cx = 0;
    const float cy = 0;

    for (int i = 0; i < num_ratio; ++i) {
        float ar = ratios[i];

        int r_w = round(base_size / sqrt(ar));
        int r_h = round(r_w * ar); //round(base_size * sqrt(ar));

        for (int j = 0; j < num_scale; ++j) {
            float scale = scales[j];

            float rs_w = r_w * scale;
            float rs_h = r_h * scale;

            float* anchor = anchors.row(i * num_scale + j);

            anchor[0] = cx - rs_w * 0.5f;
            anchor[1] = cy - rs_h * 0.5f;
            anchor[2] = cx + rs_w * 0.5f;
            anchor[3] = cy + rs_h * 0.5f;
        }
    }

    return anchors;
}

static void generate_proposals(const ncnn::Mat& anchors, int feat_stride, const ncnn::Mat& score_blob, const ncnn::Mat& bbox_blob, const ncnn::Mat& landmark_blob, float prob_threshold, std::vector<FaceObject>& faceobjects) {
    int w = score_blob.w;
    int h = score_blob.h;

    const int num_anchors = anchors.h;

    for (int q = 0; q < num_anchors; q++) {
        const float* anchor = anchors.row(q);

        const ncnn::Mat score = score_blob.channel(q + num_anchors);
        const ncnn::Mat bbox = bbox_blob.channel_range(q * 4, 4);
        const ncnn::Mat landmark = landmark_blob.channel_range(q * 10, 10);

        // shifted anchor
        float anchor_y = anchor[1];

        float anchor_w = anchor[2] - anchor[0];
        float anchor_h = anchor[3] - anchor[1];

        for (int i = 0; i < h; i++) {
            float anchor_x = anchor[0];

            for (int j = 0; j < w; j++) {
                int index = i * w + j;
                float prob = score[index];

                if (prob >= prob_threshold) {
                    // apply center size
                    float dx = bbox.channel(0)[index];
                    float dy = bbox.channel(1)[index];
                    float dw = bbox.channel(2)[index];
                    float dh = bbox.channel(3)[index];

                    float cx = anchor_x + anchor_w * 0.5f;
                    float cy = anchor_y + anchor_h * 0.5f;

                    float pb_cx = cx + anchor_w * dx;
                    float pb_cy = cy + anchor_h * dy;

                    float pb_w = anchor_w * exp(dw);
                    float pb_h = anchor_h * exp(dh);

                    float x0 = pb_cx - pb_w * 0.5f;
                    float y0 = pb_cy - pb_h * 0.5f;
                    float x1 = pb_cx + pb_w * 0.5f;
                    float y1 = pb_cy + pb_h * 0.5f;

                    FaceObject obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0 + 1;
                    obj.rect.height = y1 - y0 + 1;
                    obj.landmarks[0].x = cx + (anchor_w + 1) * landmark.channel(0)[index];
                    obj.landmarks[0].y = cy + (anchor_h + 1) * landmark.channel(1)[index];
                    obj.landmarks[1].x = cx + (anchor_w + 1) * landmark.channel(2)[index];
                    obj.landmarks[1].y = cy + (anchor_h + 1) * landmark.channel(3)[index];
                    obj.landmarks[2].x = cx + (anchor_w + 1) * landmark.channel(4)[index];
                    obj.landmarks[2].y = cy + (anchor_h + 1) * landmark.channel(5)[index];
                    obj.landmarks[3].x = cx + (anchor_w + 1) * landmark.channel(6)[index];
                    obj.landmarks[3].y = cy + (anchor_h + 1) * landmark.channel(7)[index];
                    obj.landmarks[4].x = cx + (anchor_w + 1) * landmark.channel(8)[index];
                    obj.landmarks[4].y = cy + (anchor_h + 1) * landmark.channel(9)[index];
                    obj.prob = prob;

                    faceobjects.emplace_back(obj);
                }

                anchor_x += feat_stride;
            }

            anchor_y += feat_stride;
        }
    }
}

static void nms_sorted_bboxes(const std::vector<FaceObject>& faceobjects, std::vector<int>& picked, float nms_threshold) {
    picked.clear();

    const int n = faceobjects.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; ++i) {
        areas[i] = faceobjects[i].rect.area();
    }

    for (int i = 0; i < n; ++i) {
        const FaceObject& a = faceobjects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); ++j) {
            const FaceObject& b = faceobjects[picked[j]];

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep) picked.push_back(i);
    }
}

std::vector<FaceObject> retinaface_detect(ncnn::Net& retinaface, const cv::Mat& frame) {
    const float prob_threshold = 0.8f;
    const float nms_threshold = 0.4f;

    std::vector<FaceObject> faceobjects;
    int img_w = frame.cols;
    int img_h = frame.rows;

    ncnn::Mat in = ncnn::Mat::from_pixels(frame.data, ncnn::Mat::PIXEL_BGR2RGB, img_w, img_h);

    ncnn::Extractor ex = retinaface.create_extractor();
    ex.set_light_mode(true);
    ex.input("data", in);

    std::vector<FaceObject> faceproposals;

    { // stride 32
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("face_rpn_cls_prob_reshape_stride32", score_blob);
        ex.extract("face_rpn_bbox_pred_stride32", bbox_blob);
        ex.extract("face_rpn_landmark_pred_stride32", landmark_blob);

        const int base_size = 16;
        const int feat_stride = 32;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 32.f;
        scales[1] = 16.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);

        std::vector<FaceObject> faceobjects32;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects32);

        faceproposals.insert(faceproposals.end(), faceobjects32.begin(), faceobjects32.end());
    }
    { // stride 16
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("face_rpn_cls_prob_reshape_stride16", score_blob);
        ex.extract("face_rpn_bbox_pred_stride16", bbox_blob);
        ex.extract("face_rpn_landmark_pred_stride16", landmark_blob);

        const int base_size = 16;
        const int feat_stride = 16;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 8.f;
        scales[1] = 4.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);

        std::vector<FaceObject> faceobjects16;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects16);

        faceproposals.insert(faceproposals.end(), faceobjects16.begin(), faceobjects16.end());
    }
    { // stride 8
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("face_rpn_cls_prob_reshape_stride8", score_blob);
        ex.extract("face_rpn_bbox_pred_stride8", bbox_blob);
        ex.extract("face_rpn_landmark_pred_stride8", landmark_blob);

        const int base_size = 16;
        const int feat_stride = 8;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 2.f;
        scales[1] = 1.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);

        std::vector<FaceObject> faceobjects8;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects8);

        faceproposals.insert(faceproposals.end(), faceobjects8.begin(), faceobjects8.end());
    }

    // sort all proposals by score from highest to lowest
    qsort_descent_inplace(faceproposals);

    // apply nms with nms_threshold
    std::vector<int> picked;
    nms_sorted_bboxes(faceproposals, picked, nms_threshold);

    int face_count = picked.size();

    faceobjects.resize(face_count);
    for (int i = 0; i < face_count; i++)
    {
        faceobjects[i] = faceproposals[picked[i]];

        // clip to image size
        float x0 = faceobjects[i].rect.x;
        float y0 = faceobjects[i].rect.y;
        float x1 = x0 + faceobjects[i].rect.width;
        float y1 = y0 + faceobjects[i].rect.height;

        x0 = std::max(std::min(x0, (float)img_w - 1), 0.f);
        y0 = std::max(std::min(y0, (float)img_h - 1), 0.f);
        x1 = std::max(std::min(x1, (float)img_w - 1), 0.f);
        y1 = std::max(std::min(y1, (float)img_h - 1), 0.f);

        faceobjects[i].rect.x = x0;
        faceobjects[i].rect.y = y0;
        faceobjects[i].rect.width = x1 - x0;
        faceobjects[i].rect.height = y1 - y0;
    }

    return std::move(faceobjects);
}

}

void detection_thread_func(void) {
    g_exit_detection_thread.store(false);
    std::cout << "[retina] info: starting face detection thread.\n";

    static const std::array<cv::Point2f, 5> reference = {
        cv::Point2f(38.2946f, 51.6963f),
        cv::Point2f(73.5318f, 51.5014f),
        cv::Point2f(56.0252f, 71.7366f),
        cv::Point2f(41.5493f, 92.3655f),
        cv::Point2f(70.7299f, 92.2041f)
    };

    ncnn::Net retinaface_net;
    retinaface_net.opt.use_vulkan_compute = true;
    retinaface_net.opt.num_threads = 4;
    // https://github.com/nihui/ncnn-assets/tree/master/models
    if (retinaface_net.load_param("models/retinaface/mnet.25-opt.param") != 0 ||
            retinaface_net.load_model("models/retinaface/mnet.25-opt.bin") != 0) {
        std::cerr << "[retina] error: failed to load RetinaFace model." << std::endl;
        return;
    }
    std::cout << "[retina] info: RetinaFace model loaded successfully.\n";

    while (!g_exit_detection_thread.load()) {
        cv::Mat frame;
        { std::lock_guard<std::mutex> lock(g_frame_mutex);
            if (g_frame.empty()) continue;
            frame = g_frame.clone();
        }
        
        // preprocess frame
        const int target_size = 640;
        int w = frame.cols;
        int h = frame.rows;
        float scale = std::min(target_size / (float)w, target_size / (float)h);
        int resized_w = static_cast<int>(w * scale);
        int resized_h = static_cast<int>(h * scale);
        int pad_x = (target_size - resized_w) / 2;
        int pad_y = (target_size - resized_h) / 2;
        cv::resize(frame, frame, cv::Size(resized_w, resized_h));
        cv::copyMakeBorder(frame, frame, pad_y, target_size - resized_h - pad_y, pad_x, target_size - resized_w - pad_x, cv::BORDER_CONSTANT, cv::Scalar(0,0,0));

        { std::lock_guard<std::mutex> lock(g_retina_debug_buffer_2_mutex);
            g_retina_debug_buffer_2 = frame.clone();
        }

        // detect faces
        std::vector<FaceObject> detected_faces = retinaface_detect(retinaface_net, frame);
        for (const FaceObject& fo : detected_faces) {
            // skip invalid
            cv::Rect clipped_bbox = fo.rect & cv::Rect(0, 0, frame.cols, frame.rows);
            if (clipped_bbox.width <= 0 || clipped_bbox.height <= 0) continue;

            // align face
            cv::Mat transform = cv::estimateAffinePartial2D(fo.landmarks, reference);
            cv::warpAffine(frame, frame, transform, cv::Size(112, 112), cv::INTER_LINEAR);

            { std::lock_guard<std::mutex> lock(g_retina_debug_buffer_1_mutex);
                g_retina_debug_buffer_1 = frame.clone();
            }
            
            // push to embedding buffer
            { std::lock_guard<std::mutex> lock(g_embedding_buffer_mutex);
                g_embedding_buffer.push(std::move(frame));
            }
        }
    }

    std::cout << "[retina] info: exiting detection thread.\n";
}

#endif
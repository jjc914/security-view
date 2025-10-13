# Security-View  
**Real-Time Face Recognition & Surveillance System for Raspberry Pi**  
[![GitHub Repo](https://img.shields.io/badge/github-jjc914/security--view-blue?logo=github)](https://github.com/jjc914/security-view)

---

### 📸 Overview
**Security-View** is a multi-threaded, real-time security camera system built for the **Raspberry Pi 5** in **C++**, featuring **face detection, recognition, and secure video streaming** via a lightweight web interface.  
The system captures video, detects and embeds faces, manages recordings with a circular buffer (supporting pre-recording), and serves the stream over HTTP with basic authentication.

---

### 🚀 Features
- **🧠 Face Recognition Pipeline**
  - Real-time **detection** (SCRFD) and **embedding** (MobileFaceNet / mnet.25-opt) using **ncnn**.
  - Threaded pipelines for detection, embedding, and recognition.
- **📹 Multi-Threaded Recording**
  - Continuous recording with pre-event buffering.
  - Automatic tagging of faces and metadata.
- **💾 Circular Video Buffer**
  - Always maintains recent footage.
  - Pre-recording ensures events are captured *before* detection triggers.
- **🌐 Secure Web Interface**
  - Minimalist **HTML / CSS / JS** front-end.
  - Built-in **HTTP server** with basic authentication for remote viewing.
- **🐧 Optimized for Raspberry Pi**
  - Designed for **low-power ARM devices**.
  - Utilizes **OpenCV** and **ncnn** for efficient inference.

---

### 🧩 Tech Stack
| Component | Technology |
|------------|-------------|
| Core Language | C++ 20 |
| Web Interface | HTML / CSS / JavaScript |
| Video & Image Processing | OpenCV |
| Neural Inference | Tencent ncnn |
| Authentication & Streaming | cpp-httplib |
| Target Platform | Raspberry Pi 5 (64-bit Linux) |

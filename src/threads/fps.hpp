#ifndef THREADS_FPS_HPP
#define THREADS_FPS_HPP

#include "../globals.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

void fps_thread_func(void) {
    g_exit_fps_thread.store(false);
    std::cout << "[fps] info: starting fps calculation thread.\n";
    
    while (!g_exit_fps_thread.load()) {
        int start_count = g_frame_count.load();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        int end_count = g_frame_count.load();
        g_fps.store(end_count - start_count);
    }
    std::cout << "[fps] info: exiting fps thread.\n";
}

#endif
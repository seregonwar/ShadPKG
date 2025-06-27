#pragma once
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <ctime>

inline void simple_log(const std::string& msg) {
    static std::mutex mtx;
    static std::ofstream log_file("debug_log.txt", std::ios::app);
    std::lock_guard<std::mutex> lock(mtx);

    // Timestamp
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    log_file << "[" << buf << "] " << msg << std::endl;
    log_file.flush();
    std::cout << "[LOG] " << msg << std::endl;
} 
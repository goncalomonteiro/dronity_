#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace verity {

class AutosaveScheduler {
public:
    AutosaveScheduler(std::string project_dir, std::chrono::seconds interval)
        : project_dir_(std::move(project_dir)), interval_(interval) {}
    ~AutosaveScheduler() { stop(); }

    void start() {
        if (running_.exchange(true)) return;
        worker_ = std::thread([this] { run(); });
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if (worker_.joinable()) worker_.join();
    }

private:
    void run();
    void snapshotOnce();

    std::string project_dir_;
    std::chrono::seconds interval_ {60};
    std::atomic<bool> running_ {false};
    std::thread worker_;
};

} // namespace verity


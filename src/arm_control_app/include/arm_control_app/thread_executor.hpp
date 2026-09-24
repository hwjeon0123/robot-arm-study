#pragma once

#include "rclcpp/rclcpp.hpp"
#include <rclcpp/executors/single_threaded_executor.hpp>

#include <thread>

class ExecutorThread {
public:
    explicit ExecutorThread(rclcpp::Node::SharedPtr node)
    {
        executor_.add_node(node);
        thread_ = std::thread([this]() { executor_.spin(); });
    }

    ~ExecutorThread()
    {
        try {
            executor_.cancel();
            if (thread_.joinable()) {
                thread_.join();
            }
        } catch (...) {
            // 소멸자에서 예외가 빠져나가면 std::terminate 가 불린다
        }
    }

    ExecutorThread(const ExecutorThread &) = delete;
    ExecutorThread & operator=(const ExecutorThread &) = delete;

private:
    rclcpp::executors::SingleThreadedExecutor executor_;
    std::thread thread_;
};
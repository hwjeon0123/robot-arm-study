#pragma once

#include <rclcpp/rclcpp.hpp>

class LogThrottle {
public:
    LogThrottle(double interval_secs) { interval_ = interval_secs; };
    ~LogThrottle() = default;

    void SetInterval(const double new_interval) {
        interval_ = new_interval;
    };
    bool Due() {
        auto now = steady_clock_.now();
        auto diff_time = now - last_log_time_;
        if (diff_time.seconds() > interval_) {
            last_log_time_ = now;
            return true;
        }
        return false;
    };
private:
    double interval_;
    rclcpp::Clock steady_clock_{RCL_STEADY_TIME};
    rclcpp::Time last_log_time_{0, 0, RCL_STEADY_TIME};

};
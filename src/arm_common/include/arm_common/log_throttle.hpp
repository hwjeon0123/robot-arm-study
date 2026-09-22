#pragma once

#include <rclcpp/rclcpp.hpp>

class LogThrottle {
public:
    LogThrottle(double interval_secs) { interval_ = interval_secs; };
    ~LogThrottle() = default;

    void SetInterval(const double new_interval) {
        interval_ = new_interval;
    };
    // 호출 할 때의 시간차가 시간 간격 설정값 보다 크면 시각을 갱신하므로
    // 연속으로 호출하면 시간차가 거의 없어서 첫 호출 이후에는 모두 false가 
    // 반환될 수 있다. 여러 곳에서 하나의 인스턴스를 공유하면 먼저 호출한 쪽만
    // 로그가 남고 나머지는 조용히 사라질 수 있다는 의미이다.
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
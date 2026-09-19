#include <rclcpp/rclcpp.hpp>
#include <cv_bridge/cv_bridge.hpp>  // cv_bridge:: 네임스페이스용
#include <image_transport/image_transport.hpp>
#include "opencv2/highgui.hpp"  // cv:: 네임스페이스용 (ArUco, 행렬 연산 등)
#include "opencv2/aruco.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp" // cv::cvtColor() 사용을 위해 필요

#include <stdexcept>

#define OVERHEAD_CAMERA_IMAGE_TOPIC "/overhead_camera/image_raw"

class ArmVisionSubscriber : public rclcpp::Node
{
  public:
    ArmVisionSubscriber()
    : Node("arm_vision_subscriber")
    {
        // Create dictionary for ArUco marker detection
        cv_dict_ = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
        aruco_parameters_ = cv::aruco::DetectorParameters::create();
        if (!cv_dict_ || !aruco_parameters_) {
            throw std::runtime_error("ArUco dictionary or detector parameters not created");
        }

        try {
        subscription_ = image_transport::create_subscription(
            this, OVERHEAD_CAMERA_IMAGE_TOPIC,
            [this](const sensor_msgs::msg::Image::ConstSharedPtr & msg) 
            {
                HandleImage(msg);
            },
            "raw");  // 나머지 두 매개변수는 기본값으로 두어도 된다. (QoS, callback 그룹)

        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Failed to subscribe %s: %s",
                             OVERHEAD_CAMERA_IMAGE_TOPIC, e.what());
            throw;
        }
    }

  private:
    void HandleImage(const sensor_msgs::msg::Image::ConstSharedPtr& image_msg);
    image_transport::Subscriber subscription_;
    cv::Ptr<cv::aruco::Dictionary> cv_dict_;
    cv::Ptr<cv::aruco::DetectorParameters> aruco_parameters_;
    bool marker_detected_{false};
    rclcpp::Clock steady_clock_{RCL_STEADY_TIME};
    rclcpp::Time last_log_{0, 0, RCL_STEADY_TIME};
};

void ArmVisionSubscriber::HandleImage(const sensor_msgs::msg::Image::ConstSharedPtr& image_msg)
{
    cv_bridge::CvImageConstPtr cv_const_ptr;
    try {
        // cv_bridge 로 ROS 메시지를 OpenCV Mat 으로 변환
        // 마커 검출만 하려면 toCvShare() 를 사용, 이미지에 뭔가 수정을 한다면
        // toCvCopy()를 사용`
        // cv_bridge::CvImagePtr cv_ptr;
        // cv_ptr =
        //     cv_bridge::toCvCopy(image_msg, sensor_msgs::image_encodings::MONO8);
        cv_const_ptr = cv_bridge::toCvShare(image_msg, "mono8");
    } catch (cv_bridge::Exception &e) {
        RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
        return;
    }

    // Vectors to store detected marker corners and their IDs
    std::vector<std::vector<cv::Point2f>> corners;
    std::vector<int> ids;
    
    // Detect markers in the image frame
    cv::aruco::detectMarkers(cv_const_ptr->image, cv_dict_, corners, ids,
                             aruco_parameters_);

    // Convert the grayscale image to BGR for display purposes
    cv::Mat display;
    cv::cvtColor(cv_const_ptr->image, display, cv::COLOR_GRAY2BGR);

    // If any markers are found, draw bounding boxes and IDs on the frame
    if (!ids.empty()) {
        cv::aruco::drawDetectedMarkers(display, corners, ids);
        // Add number of ids storage if you want to show this message 
        // when number of detected markers changes
        if (false == marker_detected_) {
            marker_detected_ = true;
            RCLCPP_INFO(this->get_logger(), "Detected %lu ArUco marker(s)",
                        ids.size());
        }

        auto diff_time = steady_clock_.now() - last_log_;
        if (diff_time.seconds() > 2.0) {
            last_log_ = steady_clock_.now();
            // Print the IDs of detected markers to the console
            std::ostringstream oss;
            size_t id_index = 0;
            for (const auto &id : ids) {
                oss << id << " " << corners[id_index][0] << " "
                    << corners[id_index][1] << "\n";
                id_index++;
            }
            RCLCPP_INFO(this->get_logger(), "%s", oss.str().c_str());
        }

    } else {
        if (true == marker_detected_) {
            marker_detected_ = false;
            RCLCPP_INFO(this->get_logger(), "No ArUco marker(s) detected");
        }
    }

    cv::imshow("view", display);
    cv::waitKey(10);
}

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    std::shared_ptr<ArmVisionSubscriber> node;

    try {
        node = std::make_shared<ArmVisionSubscriber>();
    } catch (const std::exception &e) {
        RCLCPP_ERROR(rclcpp::get_logger("arm_vision"), "%s", e.what());
        rclcpp::shutdown();
        return 1;
    } catch (...) {
        RCLCPP_ERROR(rclcpp::get_logger("arm_vision"), "Unknown exception occurred");
        rclcpp::shutdown();
        return 2;
    }

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
#include <rclcpp/rclcpp.hpp>
#include <cv_bridge/cv_bridge.hpp>  // cv_bridge:: 네임스페이스용
#include <image_transport/image_transport.hpp>
#include "opencv2/highgui.hpp"  // cv:: 네임스페이스용 (ArUco, 행렬 연산 등)


#define OVERHEAD_CAMERA_IMAGE_TOPIC "/overhead_camera/image_raw"

class ArmVisionSubscriber : public rclcpp::Node
{
  public:
    ArmVisionSubscriber()
    : Node("arm_vision_subscriber")
    {
        subscription_ = image_transport::create_subscription(
            this, OVERHEAD_CAMERA_IMAGE_TOPIC,
            [this](const sensor_msgs::msg::Image::ConstSharedPtr & msg) 
            {
                HandleImage(msg);
            },
            "raw");  // 나머지 두 매개변수는 기본값으로 두어도 된다. (QoS, callback 그룹)
    }

  private:
    void HandleImage(const sensor_msgs::msg::Image::ConstSharedPtr& image_msg);
    image_transport::Subscriber subscription_;
};

void ArmVisionSubscriber::HandleImage(const sensor_msgs::msg::Image::ConstSharedPtr& image_msg)
{
    try {
        // cv_bridge 로 ROS 메시지를 OpenCV Mat 으로 변환
        cv::imshow("view", cv_bridge::toCvShare(image_msg, "mono8")->image);
        cv::waitKey(10);
    } catch (const cv_bridge::Exception & e) {
        auto logger = rclcpp::get_logger("my_subscriber");
        RCLCPP_ERROR(logger, "Could not convert from '%s' to 'bgr8'.", image_msg->encoding.c_str());
    }
}

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ArmVisionSubscriber>());
    rclcpp::shutdown();
    return 0;
}
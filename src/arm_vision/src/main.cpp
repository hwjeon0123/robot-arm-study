#include <rclcpp/rclcpp.hpp>
#include <cv_bridge/cv_bridge.hpp>  // cv_bridge:: 네임스페이스용
#include <image_transport/image_transport.hpp>
#include "opencv2/highgui.hpp"  // cv:: 네임스페이스용 (ArUco, 행렬 연산 등)
#include "opencv2/aruco.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp" // cv::cvtColor() 사용을 위해 필요
#include "sensor_msgs/msg/camera_info.hpp"
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>

#include <stdexcept>
#include <memory>

#define OVERHEAD_CAMERA_IMAGE_TOPIC "/overhead_camera/image_raw"

// ArUco 마커의 길이를 상수로 정의. 
static constexpr float MARKER_LENGTH = 0.027f;

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
        /* 기본값은 CORNER_REFINE_NONE 이며 외곽선에서 얻은 정수에 가까운 좌표를 그대로 쓴다.
         시뮬레이션에서 측정해 보니 마커 폭이 46.3px 로 잡혔는데 영상에서 직접 잰 경계는 46.79px 였다.
         픽셀 계산의 정밀도 문제로 판단해 CORNER_REFINE_SUBPIX를 사용해서 정밀도를 높이니 46.72px 로 영상에 맞았다.
         다만 이것으로 오차가 다 없어지지는 않았다. 27mm 마커라면 47.73px 여야 하는데 영상 자체가 46.79px 였다. 
         이 오차는 렌더링 과정에서 검은 사각형이 실제보다 작게 그려져서 발생하는 것이어서 개선할 수 없다.
         */
        aruco_parameters_->cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;

        try {
            tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
            tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
        } catch (const std::exception & e) {
            RCLCPP_ERROR(this->get_logger(), "Failed to create tf2 buffer or listener: %s", e.what());
            throw;
        }

        try {
        subscription_ = image_transport::create_camera_subscription(
            this, OVERHEAD_CAMERA_IMAGE_TOPIC,
            [this](const sensor_msgs::msg::Image::ConstSharedPtr & image_msg,
                    const sensor_msgs::msg::CameraInfo::ConstSharedPtr & info_msg) 
            {
                HandleImage(image_msg, info_msg);
            },
            "raw");  
        } catch (const std::exception & e) {
            RCLCPP_ERROR(this->get_logger(), "Failed to subscribe %s: %s",
                             OVERHEAD_CAMERA_IMAGE_TOPIC, e.what());
            throw;
        } catch (...) {
            RCLCPP_ERROR(this->get_logger(), "Failed to subscribe %s: Unknown exception",
                             OVERHEAD_CAMERA_IMAGE_TOPIC);
            throw;
        }
    }

  private:
    image_transport::CameraSubscriber subscription_;
    cv::Ptr<cv::aruco::Dictionary> cv_dict_;
    cv::Ptr<cv::aruco::DetectorParameters> aruco_parameters_;
    bool marker_detected_{false};
    bool camera_info_received_{false};
    cv::Matx33d camera_matrix_;
    std::vector<double> dist_coeffs_;
    rclcpp::Clock steady_clock_{RCL_STEADY_TIME};
    rclcpp::Time last_log_{0, 0, RCL_STEADY_TIME};
    std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_{nullptr};
    
    void HandleImage(const sensor_msgs::msg::Image::ConstSharedPtr & image_msg,
        const sensor_msgs::msg::CameraInfo::ConstSharedPtr & info_msg);
    void DisplayMarker(cv::InputArray& image, 
        std::vector<int>& ids, std::vector<std::vector<cv::Point2f>>& corners);
};


void ArmVisionSubscriber::DisplayMarker(cv::InputArray& image, 
    std::vector<int>& ids, std::vector<std::vector<cv::Point2f>>& corners)
{
    // Convert the grayscale image to BGR for display purposes
    cv::Mat display;
    cv::cvtColor(image, display, cv::COLOR_GRAY2BGR);

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

void ArmVisionSubscriber::HandleImage(const sensor_msgs::msg::Image::ConstSharedPtr & image_msg,
    const sensor_msgs::msg::CameraInfo::ConstSharedPtr & info_msg)
{
    // Take camera info 
    if (!camera_info_received_) {
        camera_matrix_ = cv::Matx33d(
            info_msg->k[0], 0, info_msg->k[2],
            0, info_msg->k[4], info_msg->k[5],
            0, 0, 1
        );
        
        dist_coeffs_ = std::vector<double>(info_msg->d);

        camera_info_received_ = true;
        RCLCPP_INFO(this->get_logger(), "Camera info received: fx=%f, fy=%f, cx=%f, cy=%f",
                    camera_matrix_(0, 0), camera_matrix_(1, 1),
                    camera_matrix_(0, 2), camera_matrix_(1, 2));
    }

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
    std::vector<cv::Vec3d> rvecs, tvecs;
    
    // Detect markers in the image frame
    cv::aruco::detectMarkers(cv_const_ptr->image, cv_dict_, corners, ids,
                             aruco_parameters_);

    if (true == ids.empty()) 
    {
        return;  // No markers detected, exit callback
    }

    DisplayMarker(cv_const_ptr->image, ids, corners);

    cv::aruco::estimatePoseSingleMarkers(corners, MARKER_LENGTH, camera_matrix_, 
        dist_coeffs_, rvecs, tvecs);
    
    if(tvecs.size() > 0) {
        for(size_t i = 0; i < tvecs.size(); i++) {
            RCLCPP_INFO(this->get_logger(), "Marker ID: %d, Position: [%.3f, %.3f, %.3f]",
                        ids[i], tvecs[i][0], tvecs[i][1], tvecs[i][2]);
        }
    }

    geometry_msgs::msg::TransformStamped tr_stamped;
    geometry_msgs::msg::PoseStamped marker_pose_camera_frame;
    geometry_msgs::msg::PoseStamped base_link_pose; // Initialize with default values

    marker_pose_camera_frame.header = image_msg->header;
    marker_pose_camera_frame.pose.position.x = tvecs[0][0];
    marker_pose_camera_frame.pose.position.y = tvecs[0][1];
    marker_pose_camera_frame.pose.position.z = tvecs[0][2]; 
    marker_pose_camera_frame.pose.orientation.x = 0.0;
    marker_pose_camera_frame.pose.orientation.y = 0.0;
    marker_pose_camera_frame.pose.orientation.z = 0.0;
    marker_pose_camera_frame.pose.orientation.w = 1.0;

    try {

        tr_stamped = tf_buffer_->lookupTransform(
            "base_link", "overhead_camera_link_optical", 
            image_msg->header.stamp,
            rclcpp::Duration::from_seconds(0.1));

    } catch (const tf2::TransformException & ex) {
        RCLCPP_INFO(this->get_logger(),
            "Could not transform base_link to overhead_camera_link_optical: %s",
            ex.what());
        return;
    }
    
    tf2::doTransform(marker_pose_camera_frame, base_link_pose, tr_stamped);


    RCLCPP_INFO(this->get_logger(), 
        "base_link pose: %f, %f, %f", base_link_pose.pose.position.x,
        base_link_pose.pose.position.y, base_link_pose.pose.position.z);

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
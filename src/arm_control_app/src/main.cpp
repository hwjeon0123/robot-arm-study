#include <control_msgs/action/parallel_gripper_command.hpp>
#include <memory>
#include <moveit/move_group_interface/move_group_interface.hpp>
#include <moveit/planning_scene_interface/planning_scene_interface.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include "arm_control_app/arm_controller.hpp"
#include "arm_control_app/thread_executor.hpp"

#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include <exception>

// SIGINT 처리용 전역 플래그
std::atomic<bool> g_quit{false};

//시그널 핸들러: 아무 동작도 하지 않고 플래그만 세우고 즉시 빠져나옴 (안전성 보장)
void signal_handler(int signum) {
    if(SIGINT == signum || SIGTERM == signum)
    {
        g_quit = true;
    }    
}

inline void make_pose(
  geometry_msgs::msg::Pose & pose, double x, double y, double z, double qx, double qy, double qz,
  double qw)
{
    pose.position.x = x;
    pose.position.y = y;
    pose.position.z = z;
    pose.orientation.x = qx;
    pose.orientation.y = qy;
    pose.orientation.z = qz;
    pose.orientation.w = qw;
}

bool ExecutePickAndPlaceCycle(std::shared_ptr<ArmController>& arm_ctrl, 
    geometry_msgs::msg::Vector3 marker_pos, geometry_msgs::msg::Quaternion marker_rot,
    geometry_msgs::msg::Vector3 target_pos, geometry_msgs::msg::Quaternion target_rot)
{
    geometry_msgs::msg::Pose pose;

    // test_configuration 이동
    if (false == arm_ctrl->MoveToNamedTarget("test_configuration")) 
        return false;

    if(g_quit)
        return false;

    // 마커 위치 바로 위로 이동
    make_pose(pose, marker_pos.x, marker_pos.y, 0.1, 1.0, 0.0, 0.0, 0.0);
    if (false == arm_ctrl->MoveToPose(pose)) 
        return false;

    if(g_quit)
        return false;

    // 그리퍼 집기 (position=0.035, effort=20.0)
    if (false == arm_ctrl->OperateGripper(0.025, 20.0)) 
        return false;
    if(g_quit)
        return false;
    // 그리퍼 열기 (position=0.0, effort=20.0)
    if (false == arm_ctrl->OperateGripper(0.0, 20.0)) 
        return false;
    if(g_quit)
        return false;


    // 물체를 잡기 위해 내려가기
    make_pose(pose, marker_pos.x, marker_pos.y, 0.002, 1.0, 0.0, 0.0, 0.0);
    if (false == arm_ctrl->MoveToPose(pose)) 
        return false;

    if(g_quit)
        return false;

    // 그리퍼 집기 (position=0.035, effort=20.0)
    if (false == arm_ctrl->OperateGripper(0.025, 20.0)) 
        return false;

    if(g_quit)
        return false;

    // 수직 상승
    make_pose(pose, marker_pos.x, marker_pos.y, 0.1, 1.0, 0.0, 0.0, 0.0);
    if (false == arm_ctrl->MoveToPose(pose)) 
        return false;

    if(g_quit)
        return false;

    // 목표 지점 위로 이동
    make_pose(pose, target_pos.x, target_pos.y, 0.1, 1.0, 0.0, 0.0, 0.0);
    if (false == arm_ctrl->MoveToPose(pose)) 
        return false;

    if(g_quit)
        return false;

    // 내려놓을 위치로 내려가기
    make_pose(pose, target_pos.x, target_pos.y, 0.002, 1.0, 0.0, 0.0, 0.0);
    if (false == arm_ctrl->MoveToPose(pose)) 
        return false;

    if(g_quit)
        return false;

    // 그리퍼 열기 (position=0.0, effort=20.0)
    if (false == arm_ctrl->OperateGripper(0.0, 20.0)) 
        return false;

    return true;
}

int main(int argc, char * argv[])
{
    rclcpp::NodeOptions node_opt;

    rclcpp::InitOptions init_options;
    // ROS2 자체 시그널 핸들러(Ctrl+C 처리) 비활성화
    init_options.shutdown_on_signal = false;
    // Initialize ROS and create the Node
    rclcpp::init(argc, argv, init_options);

    // 시그널 처리 등록
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    // 파이프 에러 무시
    std::signal(SIGPIPE, SIG_IGN);

    // main 함수가 끝날 때(성공이든 에러 반환이든) 무조건 shutdown()을 대신 불러줌
    std::shared_ptr<void> rclcpp_guard(nullptr, [](void*) { rclcpp::shutdown(); });

    node_opt.automatically_declare_parameters_from_overrides(true);
    
    // Create a ROS logger
    auto const logger = rclcpp::get_logger("arm_control_app");

    try {
        auto arm_controller = std::make_shared<ArmController>(node_opt);

        // node를 루프에서 실행시킬 executor를 만들고, 별도 스레드에서 스핀을 먼저
        // 시작한다. MoveGroupInterface 생성자는 내부적으로 서비스 응답을
        // 기다리는데, 그 응답을 처리해줄 스핀이 안 돌고 있으면 여기서 영원히
        // 멈춘다.
        ExecutorThread spinner(arm_controller);

        if(false == arm_controller->InitMoveIt(10.0, 10))
        {
            RCLCPP_ERROR(logger, "Failed to init MoveIt");
            return 1;
        }

        using namespace std::chrono_literals;
        rclcpp::TimerBase::SharedPtr quit_mon_timer;
        quit_mon_timer = arm_controller->create_wall_timer(100ms, [arm_controller, &quit_mon_timer]() -> 
            void {
                if(g_quit)
                {
                    arm_controller->Stop();
                    // Stop timer
                    quit_mon_timer->cancel();
                }
            }
        );

        // TF listener. Create after spinner run.
        std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
        std::shared_ptr<tf2_ros::TransformListener> tf_listener_{nullptr};
        geometry_msgs::msg::TransformStamped tf_stamped;

        geometry_msgs::msg::Vector3 target_loc;
        geometry_msgs::msg::Quaternion target_rot;
        target_loc.x = 0.6;
        target_loc.y = 0.6;
        target_loc.z = 0.002;
        target_rot.x = 0.0;
        target_rot.y = 0.0;
        target_rot.z = 0.0;
        target_rot.w = 1.0;

        try {
            tf_buffer_ = std::make_unique<tf2_ros::Buffer>(arm_controller->get_clock());
            tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        } catch (const std::exception & e) {
            RCLCPP_ERROR(logger, "Failed to create tf2 buffer, listener or broadcaster: %s", e.what());
            throw;
        }

        try {
             tf_stamped = tf_buffer_->lookupTransform(
                "base_link", "marker_0", tf2::TimePointZero,
                tf2::durationFromSec(1.0));
        } catch (const tf2::TransformException &ex) {
            RCLCPP_INFO(logger,
                "Could not transform base_link to "
                "marker_0: %s", ex.what());
            throw;
        }

        
        if (false == ExecutePickAndPlaceCycle(arm_controller, 
                tf_stamped.transform.translation, tf_stamped.transform.rotation,
                target_loc, target_rot)) {
            RCLCPP_ERROR(logger, "Pick and place task failed");
            // Do not return to make arm return to the test configuration position
        }

        // 시작 위치(test_configuration)로 복귀
        arm_controller->MoveToNamedTarget("test_configuration");

    } catch (const std::exception & e) {
        RCLCPP_ERROR(logger, "Error in main: %s", e.what());
    }

    return 0;
}

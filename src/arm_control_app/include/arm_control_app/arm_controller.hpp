#pragma once

#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <moveit/move_group_interface/move_group_interface.hpp>
#include <moveit/planning_scene_interface/planning_scene_interface.hpp>
#include <geometry_msgs/msg/pose.hpp>
#include <control_msgs/action/parallel_gripper_command.hpp>

class ArmController : public rclcpp::Node {
public:
    explicit ArmController(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
    ~ArmController();

    bool InitMoveIt(double planning_time, unsigned int num_planning_attemps);
    bool MoveToNamedTarget(const std::string& target_name);
    bool MoveToPose(const geometry_msgs::msg::Pose& target_pose);
    bool OperateGripper(double position, double effort);
    void Stop();

private:
    bool SetupCollisionObject();
    std::shared_ptr<moveit::planning_interface::MoveGroupInterface> move_group_;
    rclcpp_action::Client<control_msgs::action::ParallelGripperCommand>::SharedPtr gripper_client_;
    std::atomic<bool> quit_flag_{false}; 
};

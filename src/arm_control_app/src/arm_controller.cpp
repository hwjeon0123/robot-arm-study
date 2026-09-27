#include "arm_control_app/arm_controller.hpp"

ArmController::ArmController(const rclcpp::NodeOptions &options)
: Node("arm_control_node", options)
  , move_group_(nullptr)
{
}

ArmController::~ArmController()
{
    if(nullptr != move_group_)
    {
        move_group_->stop();
        move_group_ = nullptr;
    }
}

bool ArmController::InitMoveIt(double planning_time, unsigned int num_planning_attempts)
{
    if(nullptr != move_group_)
    {
        // Already initialized;
        return true;
    }

    // MoveGroupInterface 생성. 두 번째 인자 "ur_manipulator"는 SRDF에서
    // 정의한 planning group 이름과 정확히 같아야 한다.
    try {
        move_group_ = std::make_shared<moveit::planning_interface::MoveGroupInterface>(
                shared_from_this(), "ur_manipulator");
 
    } catch (const std::exception& e) {
        RCLCPP_ERROR(this->get_logger(), "Failed to allocate move group: %s", e.what());
        return false;
    }

    if(false == move_group_->setEndEffectorLink("tcp_link"))
    {
        RCLCPP_ERROR(this->get_logger(), "Failed to set end effector to tcp_link");
        return false;        
    }

    try {
        // Gripper Client 초기화
        gripper_client_ = rclcpp_action::create_client<control_msgs::action::ParallelGripperCommand>(
            this, "gripper_controller/gripper_cmd");
    } catch (const std::exception& e)
    {
        RCLCPP_ERROR(this->get_logger(), "Failed to create gripper action client: %s",
                e.what());
        return false;        
    }

    if(false == SetupCollisionObject())
    {
        RCLCPP_ERROR(this->get_logger(), "Failed to create ground collision obejct");
        return false;        
    }

    move_group_->setPlanningTime(planning_time);
    move_group_->setNumPlanningAttempts(num_planning_attempts);

    return true;
}

bool ArmController::SetupCollisionObject()
{
    // Ground object를 CollisionObject로 만들어서 PlanningScene에 추가.
    // 이렇게 해야 바닥에 충돌하지 않고 로봇팔이 움직이도록 MoveIt이 계획을
    // 세운다.
    moveit::planning_interface::PlanningSceneInterface iface_ps;
    moveit_msgs::msg::CollisionObject ground_collision_object;

    ground_collision_object.id = "ground";
    ground_collision_object.header.frame_id = "world";
    ground_collision_object.primitives.resize(1);
    ground_collision_object.primitives[0].type = shape_msgs::msg::SolidPrimitive::BOX;
    ground_collision_object.primitives[0].dimensions = { 1.0, 1.0, 0.005 };
    ground_collision_object.primitive_poses.resize(1);
    ground_collision_object.primitive_poses[0].position.x = 0.0;
    ground_collision_object.primitive_poses[0].position.y = 0.0;
    ground_collision_object.primitive_poses[0].position.z = -0.0025;
    ground_collision_object.operation = moveit_msgs::msg::CollisionObject::ADD;

    if (!iface_ps.applyCollisionObject(ground_collision_object)) {
        RCLCPP_ERROR(this->get_logger(), "Failed to apply ground collision object");
        return false;
    }

    return true;
}

bool ArmController::MoveToNamedTarget(const std::string& target_name)
{
    move_group_->setNamedTarget(target_name);
    auto ok = static_cast<bool>(move_group_->move());
    if (!ok) {
        RCLCPP_ERROR(this->get_logger(), "Move to '%s' failed", target_name.c_str());
        return false;
    }
    RCLCPP_INFO(this->get_logger(), "Move to '%s' succeeded", target_name.c_str());
    return true;
}

bool ArmController::MoveToPose(const geometry_msgs::msg::Pose& target_pose)
{
    move_group_->setPoseTarget(target_pose);
    auto ok = static_cast<bool>(move_group_->move());
    if (!ok) {
        RCLCPP_ERROR(this->get_logger(), "Move to position failed");
        return false;
    }
    RCLCPP_INFO(this->get_logger(), "Move to position succeeded");
    return true;
}

bool ArmController::OperateGripper(double position, double effort)
{
    if (!gripper_client_->wait_for_action_server(std::chrono::seconds(10))) {
        RCLCPP_ERROR(this->get_logger(), "Cannot find gripper action server.");
        return false;
    }

    control_msgs::action::ParallelGripperCommand::Goal goal;
    goal.command.name = {"finger1_joint"};
    goal.command.position = {position};
    goal.command.effort = {effort};

    rclcpp_action::Client<control_msgs::action::ParallelGripperCommand>::SendGoalOptions options;
    
    using GoalHandle = rclcpp_action::ClientGoalHandle<control_msgs::action::ParallelGripperCommand>;
    options.goal_response_callback = [this](const GoalHandle::SharedPtr &goal_handle) {
        if (!goal_handle) {
            RCLCPP_ERROR(this->get_logger(), "Goal was rejected by server");
        } else {
            RCLCPP_INFO(this->get_logger(), "Goal accepted by server, waiting for result");
        }
    };
    options.result_callback = [this](const GoalHandle::WrappedResult &result) {
        if (result.code != rclcpp_action::ResultCode::SUCCEEDED) {
            RCLCPP_ERROR(this->get_logger(), "Goal failed with code: %d", static_cast<int>(result.code));
            return;
        }
        RCLCPP_INFO(this->get_logger(), "stalled=%d reached_goal=%d", result.result->stalled, result.result->reached_goal);
    };

    auto goal_handle_future = gripper_client_->async_send_goal(goal, options);
    while (rclcpp::ok() && (false == quit_flag_)) {
        if (std::future_status::ready == 
                goal_handle_future.wait_for(std::chrono::milliseconds(100))) {
            break;
        }
    }

    if(false == rclcpp::ok() || (true == quit_flag_)) {
        RCLCPP_ERROR(this->get_logger(), "Gripper operation cancelled before accept");
        return false;
    }
         
    auto goal_handle = goal_handle_future.get();
    if (!goal_handle) {
        RCLCPP_ERROR(this->get_logger(), "Gripper goal was rejected");
        return false;
    }

    auto result_future = gripper_client_->async_get_result(goal_handle);
    bool is_cancelled = false;

    while (rclcpp::ok() && !quit_flag_) {
        if (std::future_status::ready == 
                result_future.wait_for(std::chrono::milliseconds(100))) {
            break;
        }
    }

    // 결과를 기다리는 도중 중단 요청이 오면 취소 발송
    if (!rclcpp::ok() || quit_flag_) {
        gripper_client_->async_cancel_goal(goal_handle);
        is_cancelled = true;

        // 취소를 보낸 뒤 서버 처리가 완전히 끝날 때까지 1번 더 기다림
        while (rclcpp::ok()) {
            if (std::future_status::ready == 
                    result_future.wait_for(std::chrono::milliseconds(100))) {
                break;
            }
        }
    }

    if (is_cancelled) {
        RCLCPP_ERROR(this->get_logger(), "Gripper operation cancelled during execution");
        return false;
    }
    
    auto result = result_future.get();
    return (result.code == rclcpp_action::ResultCode::SUCCEEDED);
}

void ArmController::Stop()
{
    quit_flag_ = true;
    if(nullptr != move_group_)
    {
        move_group_->stop();
    }
}

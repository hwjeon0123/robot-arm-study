#include <rclcpp/rclcpp.hpp>
#include <moveit/move_group_interface/move_group_interface.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <moveit/planning_scene_interface/planning_scene_interface.hpp>
#include <control_msgs/action/parallel_gripper_command.hpp>


#include <memory>
#include <thread>

static inline int finish(int retcode, std::thread & spinner)
{
  rclcpp::shutdown();
  spinner.join();
  return retcode;
}

inline void make_pose( geometry_msgs::msg::Pose& pose,
  double x, double y, double z, double qx, double qy, double qz, double qw)
{
  pose.position.x = x;
  pose.position.y = y;
  pose.position.z = z;
  pose.orientation.x = qx;
  pose.orientation.y = qy;
  pose.orientation.z = qz;
  pose.orientation.w = qw;
}

int main(int argc, char * argv[])
{
  // Initialize ROS and create the Node
  rclcpp::init(argc, argv);
  // NodeOptions에 automatically_declare_parameters_from_overrides(true) 설정 안 하면 MoveIt 파라미터를 못 읽어서 초기화 실패
  auto const node = std::make_shared<rclcpp::Node>(
    "arm_control_app",
    rclcpp::NodeOptions().automatically_declare_parameters_from_overrides(true)
  );

  // Create a ROS logger
  auto const logger = rclcpp::get_logger("arm_control_app");

  // node를 루프에서 실행시킬 executor를 만들고, 별도 스레드에서 스핀을 먼저 시작한다.
  // MoveGroupInterface 생성자는 내부적으로 서비스 응답을 기다리는데,
  // 그 응답을 처리해줄 스핀이 안 돌고 있으면 여기서 영원히 멈춘다.
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  auto spinner = std::thread([&executor]() { executor.spin(); });

  // MoveGroupInterface 생성. 두 번째 인자 "ur_manipulator"는 SRDF에서
  // 정의한 planning group 이름과 정확히 같아야 한다.
  moveit::planning_interface::MoveGroupInterface move_group_interface(node, "ur_manipulator");
  // EndEffectorLink를 "tcp_link"로 설정. (SRDF에서 정의한 이름과 정확히 같아야 한다.)
  move_group_interface.setEndEffectorLink("tcp_link");

  // 프로그램 종료 시 MoveGroupInterface를 정리하도록 on_shutdown에 등록.
  rclcpp::on_shutdown([&move_group_interface]() {
      move_group_interface.stop();
  });

  // Ground object를 CollisionObject로 만들어서 PlanningScene에 추가.
  // 이렇게 해야 바닥에 충돌하지 않고 로봇팔이 움직이도록 MoveIt이 계획을 세운다.
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
  if(false == iface_ps.applyCollisionObject(ground_collision_object)) {
    RCLCPP_ERROR(logger, "Failed to apply ground collision object");
  } 

  // 계획 수립에 최대 30초까지 허용
  move_group_interface.setPlanningTime(30.0);
  move_group_interface.setNumPlanningAttempts(20);

  // SRDF의 group_state 중 "home"으로 목표를 설정하니 반복 테스트 때 문제가 많아서 test_configuration으로 변경.
  move_group_interface.setNamedTarget("test_configuration");
  
  // 계획을 세우고 바로 실행까지 한 번에 수행. (계획만 세우고 싶으면 move_group_interface.plan() 사용)
  auto ok = static_cast<bool>(move_group_interface.move());
  if (!ok) {
    RCLCPP_ERROR(logger, "Move to 'test_configuration' failed");
    return finish(-1, spinner);
  }

  RCLCPP_INFO(logger, "Move to 'home' succeeded");

  geometry_msgs::msg::Pose target_pose;
  control_msgs::action::ParallelGripperCommand::Goal goal;
  using gripper_action_client_t = rclcpp_action::Client<control_msgs::action::ParallelGripperCommand>::SharedPtr;
  gripper_action_client_t action_client =
    rclcpp_action::create_client<control_msgs::action::ParallelGripperCommand>(node, 
      "gripper_controller/gripper_cmd");

  rclcpp_action::Client<control_msgs::action::ParallelGripperCommand>::SendGoalOptions options;
  using GoalHandle = rclcpp_action::ClientGoalHandle<control_msgs::action::ParallelGripperCommand>;

  // Let us know if the goal is accepted/rejected (this is the current code)
  options.goal_response_callback =
    [=](const GoalHandle::SharedPtr & goal_handle) {
      if (!goal_handle) {
        RCLCPP_ERROR(logger, "Goal was rejected by server");
        return;
      } else {
        RCLCPP_INFO(logger, "Goal accepted by server, waiting for result");
      }
    };
    
  // (Optional) Let us know about intermediate feedback during the action
  options.feedback_callback =
    [=](GoalHandle::SharedPtr , 
      const std::shared_ptr<const control_msgs::action::ParallelGripperCommand::Feedback> feedback) {
        auto it = std::find(feedback->state.name.begin(), feedback->state.name.end(), "finger1_joint");
        if (it != feedback->state.name.end()) {
          size_t idx = std::distance(feedback->state.name.begin(), it);
          //double pos = feedback->state.position[idx];

          // Let us know about intermediate feedback during the action
          RCLCPP_INFO(logger, "Feedback: position=%.3f effort=%.3f",
            feedback->state.position[idx], feedback->state.effort[idx]
          );
        }
    };

  // The final result — stalled/reached_goal
  options.result_callback = [=](const GoalHandle::WrappedResult &result) {
    if (result.code != rclcpp_action::ResultCode::SUCCEEDED) {
      // Check if the server aborted/cancelled the goal
      RCLCPP_ERROR(logger, "Goal failed with code: %d",
                   static_cast<int>(result.code));
      return;
    }
    auto it = std::find(result.result->state.name.begin(),
                        result.result->state.name.end(), "finger1_joint");
    if (it != result.result->state.name.end()) {
      size_t idx = std::distance(result.result->state.name.begin(), it);
      double pos = result.result->state.position[idx];
      RCLCPP_INFO(logger, "Final result: position=%.3f effort=%.3f", pos,
                  result.result->state.effort[idx]);
    }

    RCLCPP_INFO(logger, "stalled=%d reached_goal=%d", result.result->stalled,
                result.result->reached_goal);
  };

  // 또는 Pose를 직접 설정할 수도 있다
  // RViz에서 MotionPlanning 화면에서 "Joints" 탭을 선택하고, 조인트를 움직여서
  // 원하는 위치로 이동 시킨후 Displays 화면에서 Planning Path 항목 아래의
  // links를 선택한 뒤에 tcp link의 값을 보면  Position과 Orientation이 나온다.
  // 그 값을 그대로 넣으면 된다. 
  // 먼저 타겟 위치의 바로 위로 이동
  make_pose(target_pose, 0.4, 0.6, 0.1, 1.0, 0.0, 0.0, 0.0);
  move_group_interface.setPoseTarget(target_pose);
  ok = static_cast<bool>(move_group_interface.move());

  if (!ok) {
    RCLCPP_ERROR(logger, "Move to position failed");
    return finish(-1, spinner);
  }

  RCLCPP_INFO(logger, "Move to above position of object succeeded");

  // 물체를 잡기 위해 타겟 위치로 이동
  make_pose(target_pose, 0.4, 0.6, 0.002, 1.0, 0.0, 0.0, 0.0);
  move_group_interface.setPoseTarget(target_pose);
  ok = static_cast<bool>(move_group_interface.move());
  if (!ok) {
    RCLCPP_ERROR(logger, "Move to position failed");
    return finish(-1, spinner);
  }

  RCLCPP_INFO(logger, "Move to the position to grasp object");

  // 그리퍼로 집기
  goal.command.name = {"finger1_joint"};
  goal.command.position = {0.025};
  goal.command.effort = {20.0};   // optional
    
  if (false == action_client->wait_for_action_server(std::chrono::seconds(10))) {
    RCLCPP_ERROR(logger, "Cannot find gripper action server.");
    return finish(-1, spinner);
  }

  auto goal_handle_future = action_client->async_send_goal(goal, options);
  auto goal_handle = goal_handle_future.get();   // 서버가 accept/reject 할 때까지 대기
  if (!goal_handle) {
    RCLCPP_ERROR(logger, "Gripper goal was rejected");
    return finish(-1, spinner);
  }
  
  auto result_future = action_client->async_get_result(goal_handle);
  result_future.get();   // stalled/reached_goal 결과가 올 때까지 대기

  // 물체를 내려 놓을 좌표의 위로 이동
  make_pose(target_pose, 0.6, 0.4, 0.1, 1.0, 0.0, 0.0, 0.0);
  move_group_interface.setPoseTarget(target_pose);
  ok = static_cast<bool>(move_group_interface.move());
  if (!ok) {
    RCLCPP_ERROR(logger, "Move to position failed");
    return finish(-1, spinner);
  }

  RCLCPP_INFO(logger, "Move to above position to drop object");

  make_pose(target_pose, 0.6, 0.4, 0.002, 1.0, 0.0, 0.0, 0.0);
  move_group_interface.setPoseTarget(target_pose);
  ok = static_cast<bool>(move_group_interface.move());

  if (!ok) {
    RCLCPP_ERROR(logger, "Move to position failed");
    return finish(-1, spinner);
  }

  RCLCPP_INFO(logger, "Move to position to drop object");

  // 그리퍼 열기
  goal.command.name = {"finger1_joint"};
  goal.command.position = {0.0};
  goal.command.effort = {20.0};   // optional
  
  goal_handle_future = action_client->async_send_goal(goal, options);
  goal_handle = goal_handle_future.get();   // 서버가 accept/reject 할 때까지 대기
  if (!goal_handle) {
    RCLCPP_ERROR(logger, "Gripper goal was rejected");
    return finish(-1, spinner);
  }
  
  result_future = action_client->async_get_result(goal_handle);
  result_future.get();   // stalled/reached_goal 결과가 올 때까지 대기

  move_group_interface.setNamedTarget("test_configuration");
  
  // 시작 위치로 이동
  ok = static_cast<bool>(move_group_interface.move());
  if (!ok) {
    RCLCPP_ERROR(logger, "Move back to 'test_configuration' failed");
    return finish(-1, spinner);
  }

  rclcpp::shutdown();
  // 스핀 스레드를 정리하고 종료.
  spinner.join();

  return 0;
}

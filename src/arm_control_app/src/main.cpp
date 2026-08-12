#include <rclcpp/rclcpp.hpp>
#include <moveit/move_group_interface/move_group_interface.hpp>

#include <memory>
#include <thread>

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
  // MoveGroupInterface 생성자는 내부적으로 서비스 응답을 "기다리는데",
  // 그 응답을 처리해줄 스핀이 안 돌고 있으면 여기서 영원히 멈춘다.
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  auto spinner = std::thread([&executor]() { executor.spin(); });

  // Shutdown ROS
  // MoveGroupInterface 생성. 두 번째 인자 "ur_manipulator"는 SRDF에서
  // 정의한 planning group 이름과 정확히 같아야 한다.
  moveit::planning_interface::MoveGroupInterface move_group_interface(node, "ur_manipulator");

  // SRDF의 group_state 중 "home"으로 목표를 설정.
  move_group_interface.setNamedTarget("home");
  
  // 계획을 세우고 바로 실행까지 한 번에 수
  auto ok = static_cast<bool>(move_group_interface.move());
  if (!ok) {
    RCLCPP_ERROR(logger, "Move to 'home' failed");
  }

  if(true == ok) {
    // 또는 Pose를 직접 설정할 수도 있다
    geometry_msgs::msg::Pose target_pose;
    target_pose.position.x = 0.4;
    target_pose.position.y = 0.1;
    target_pose.position.z = 0.4;
    target_pose.orientation.x = 0.0;
    target_pose.orientation.y = 0.0;
    target_pose.orientation.z = 0.0;
    target_pose.orientation.w = 1.0;
    move_group_interface.setPoseTarget(target_pose);
    ok = static_cast<bool>(move_group_interface.move());
  }
  
  if (!ok) {
    RCLCPP_ERROR(logger, "Move to position failed");
  }

  rclcpp::shutdown();
  // 스핀 스레드를 정리하고 종료.
  spinner.join();

  return 0;
}

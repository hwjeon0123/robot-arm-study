from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    OpaqueFunction,
    RegisterEventHandler,
)
from launch.conditions import IfCondition, UnlessCondition
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import (
    Command,
    FindExecutable,
    LaunchConfiguration,
    PathJoinSubstitution,
    IfElseSubstitution,
)
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.parameter_descriptions import ParameterFile
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    # Initialize launch configuration variables
    ur_type = LaunchConfiguration("ur_type")
    use_sim_time = LaunchConfiguration("use_sim_time", default="true")
    rviz_gui = LaunchConfiguration("rviz_gui", default="true")
    world_file = LaunchConfiguration("world_file")

    # ① xacro를 실행해 URDF 문자열을 만든다 (launch 실행 시점에 평가)
    robot_description_content = Command([
        FindExecutable(name="xacro"), " ",
        PathJoinSubstitution(
            [FindPackageShare("arm_description"), "urdf", "ur5e.urdf.xacro"]
        ),
        " ", "ur_type:=", ur_type,
        " ", "controllers_yaml:=", 
        PathJoinSubstitution(
            [FindPackageShare("arm_bringup"), "config", "arm_controllers.yaml"]
        ),
    ])
    robot_description = {"robot_description": ParameterValue(robot_description_content, value_type=str)}

    # ② robot_state_publisher: URDF를 /robot_description 토픽 + TF로 발행
    robot_state_publisher_node = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="screen",
        parameters=[robot_description, {"use_sim_time": use_sim_time}],
    )

    joint_state_publisher_gui_spawner = Node(
        package="joint_state_publisher_gui",
        executable="joint_state_publisher_gui",
        arguments=[],
    )

    rviz_config_file = PathJoinSubstitution(
        [FindPackageShare("arm_bringup"), "config", "check_gripper.rviz"]
    )
    rviz_spawner = Node(
        package="rviz2",
        executable="rviz2",
        arguments=["-d", rviz_config_file],
    )

    return LaunchDescription([
        DeclareLaunchArgument("ur_type", default_value="ur5e"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("world_file", default_value="default.sdf"),
        robot_state_publisher_node,
        joint_state_publisher_gui_spawner,
        rviz_spawner,
    ])

    

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    OpaqueFunction,
    RegisterEventHandler,
    SetEnvironmentVariable,
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
    gazebo_gui = LaunchConfiguration("gazebo_gui")
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

    gz_launch_description = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [FindPackageShare("ros_gz_sim"), "/launch/gz_sim.launch.py"]
        ),
        launch_arguments={
            "gz_args": IfElseSubstitution(
                gazebo_gui,
                if_value=[" -r -v 4 --physics-engine gz-physics-bullet-featherstone-plugin ", world_file],
                else_value=[" -s -r -v 4 --physics-engine gz-physics-bullet-featherstone-plugin ", world_file],
            )
        }.items(),
    )

    # GZ nodes
    gz_spawn_entity = Node(
        package="ros_gz_sim",
        executable="create",
        output="screen",
        arguments=[
            "-string",
            robot_description_content,
            "-name",
            "my_robot_arm",
            "-allow_renaming",
            "true",
        ],
    )

    # Gazebo 가 SDF 안의 model:// URI 를 해석할 때 뒤지는 경로.
    # 지정하지 않으면 board/model.sdf 의 텍스처(model://board/materials/...)를
    # 찾지 못해 마커가 회색 판으로 나온다. Gazebo 기동 전에 설정돼야 하므로
    # LaunchDescription 목록의 맨 앞에 둔다.
    set_gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=PathJoinSubstitution([FindPackageShare("arm_bringup"), "models"]),
    )

    # 픽업용 대상물 생성 (위치/자세는 아래 -x/-y/-z 로 지정)
    board_spawn_entity = Node(
        package="ros_gz_sim",
        executable="create",
        output="screen",
        arguments=[
            "-file",
            PathJoinSubstitution(
                [FindPackageShare("arm_bringup"), "models", "board", "model.sdf"]
            ),
            "-name",
            "board",
            "-x", "0.4",
            "-y", "0.6",
            "-z", "0.0025",
        ],
    )

    # Joint state broadcaster
    joint_state_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_state_broadcaster", "--controller-manager", "/controller_manager"],
    )

    # Joint trajectory controller 
    joint_trajectory_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_trajectory_controller", "--controller-manager", "/controller_manager"],
    )

    # Gripper controller spawner
    gripper_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["gripper_controller", "--controller-manager", "/controller_manager"],
    )

    clock_bridge = Node(
    package="ros_gz_bridge",
    executable="parameter_bridge",
    arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"],
    output="screen",
)

    return LaunchDescription([
        set_gz_resource_path,
        DeclareLaunchArgument("ur_type", default_value="ur5e"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("gazebo_gui", default_value="true"),
        # world 파일을 새로 생성한 파일로 변경
        DeclareLaunchArgument(
            "world_file",
            default_value=PathJoinSubstitution(
                [FindPackageShare("arm_bringup"), "worlds", "arm_study.sdf"]
            ),
        ),
        robot_state_publisher_node,
        clock_bridge,
        gz_spawn_entity,
        board_spawn_entity,
        joint_state_broadcaster_spawner,
        joint_trajectory_controller_spawner,
        gripper_controller_spawner,
        gz_launch_description,
    ])

    

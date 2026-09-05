from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
from moveit_configs_utils import MoveItConfigsBuilder


def generate_launch_description():
    # 우리 SRDF는 arm_description 패키지에 있다. MoveItConfigsBuilder의 기준
    # 패키지(arm_bringup)와 다른 곳이므로 절대경로로 직접 짚어준다.
    arm_description_share = Path(get_package_share_directory("arm_description"))

    # kinematics / joint_limits는 로봇 형상과 무관한 솔버 설정이라 새로 만들지
    # 않고 ur_moveit_config의 것을 그대로 재사용한다.
    ur_moveit_config_share = Path(get_package_share_directory("ur_moveit_config"))

    moveit_config = (
        MoveItConfigsBuilder(robot_name="ur5e", package_name="arm_bringup")
        # robot_description(URDF)는 일부러 지정하지 않는다 — 파일을 못 찾으면
        # move_group이 알아서 /robot_description 토픽을 구독한다.
        # (robot_state_publisher가 이미 그 토픽에 뿌리고 있음)
        .robot_description_semantic(
            arm_description_share / "srdf" / "arm_study.srdf.xacro",
            {"name": "ur5e"},
        )
        .robot_description_kinematics(
            #ur_moveit_config_share / "config" / "kinematics.yaml"
            Path(get_package_share_directory("arm_bringup")) / "config" / "kinematics.yaml"
        )
        .joint_limits(ur_moveit_config_share / "config" / "joint_limits.yaml")
        # 기본값(load_all=True)으로 두면 pilz까지 자동으로 끌려와서
        # pilz_cartesian_limits.yaml을 요구하며 죽는다. 지금은 OMPL 하나만 쓴다.
        .planning_pipelines(pipelines=["ompl"])
        # 인자 없이 호출 → arm_bringup/config 안에서 moveit_controllers.yaml을
        # 자동으로 찾아 읽는다.
        .trajectory_execution()
        .to_moveit_configs()
    )

    move_group_node = Node(
        package="moveit_ros_move_group",
        executable="move_group",
        output="screen",
        parameters=[
            moveit_config.to_dict(),
            {"use_sim_time": True},
            {"publish_robot_description_semantic": True }
        ],
    )

    # MoveIt 플러그인(Plan/Execute 버튼, 궤적 미리보기)이 이미 구성된 rviz 화면
    # 배치 파일. 로봇마다 새로 만들 이유가 없는 화면 구성이라 ur_moveit_config
    # 것을 그대로 가리켜서 쓴다.
    rviz_config_file = ur_moveit_config_share / "config" / "moveit.rviz"
    rviz_node = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2_moveit",
        output="log",
        arguments=["-d", str(rviz_config_file)],
        parameters=[
            # move_group과 마찬가지로 robot_description은 비어 있다 —
            # rviz도 /robot_description 토픽을 구독해서 채운다.
            moveit_config.robot_description,
            moveit_config.robot_description_semantic,
            moveit_config.robot_description_kinematics,
            moveit_config.planning_pipelines,
            moveit_config.joint_limits,
            {"use_sim_time": True},
        ],
    )

    return LaunchDescription([move_group_node, rviz_node])

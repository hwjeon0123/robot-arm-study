cmake_minimum_required(VERSION 3.8)
project(arm_vision)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
# OpenCV 패키지는 시스템 라이브러리 패키지
find_package(OpenCV REQUIRED)
find_package(ament_cmake_auto REQUIRED)
find_package(arm_common REQUIRED)

add_executable(arm_vision src/main.cpp)

# 위의 ament_auto_find_build_dependencies() 가 아래의 수동 의존성 입력을 대체할 지 확인 필요
ament_target_dependencies(arm_vision
  rclcpp 
  cv_bridge
  sensor_msgs
  image_transport
  geometry_msgs
  tf2_ros
  tf2_geometry_msgs
  arm_common
)

#OpenCV 헤더 파일 경로 연결 (ROS 패키지가 아니므로 따로 연결 필요)
target_include_directories(arm_vision PRIVATE
  ${OpenCV_INCLUDE_DIRS}
)

# OpenCV 라이브러리 파일(lib) 연결
target_link_libraries(arm_vision
  ${OpenCV_LIBRARIES}
)

install(TARGETS
  arm_vision
  DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()

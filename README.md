# robot-arm-study

ROS 2 Jazzy + Gazebo로 로봇팔 픽앤플레이스를 비전과 AI로 고도화해 가는 방법에 대한 
학습 기록.
Headless Ubuntu에 Podman 컨테이너 환경을 기반으로 하고 Nvidia graphic card의 3차원
하드웨어 가속을 사용한다. 
원격 접속을 통한 사용 환경이므로 vglrun을 이용한 3차원 가속을 컨테이너 내부에서 사용할 
수 있도록 하였다.

원격 접속에는 **Sunshine**을 쓴다. xrdp나 VNC는 GPU 가속을 받지 못해 Gazebo나 RViz를
실질적으로 돌릴 수 없기 때문이다. Sunshine은 가상 모니터로 Xorg를 띄우고 화면을 GPU로
인코딩해 스트리밍하므로 3차원 가속이 유지된다.

## 학습 목적

"물건을 집어 지정된 위치로 옮긴다"를 직접 구성해 보는 것이 목적이다. 이미 만들어져 
제공되는 URDF로 로봇을 세우는 것부터 시작해 컨트롤러, 모션 플래닝, 그리퍼, 비전 인식까지
한 단계씩 쌓아간다. 그리고 최종적으로 어떻게 AI를 여기에 적용시키는지 그 방법에 대한 
학습까지 목표로 한다.
남이 만든 데모를 실행해 보는 것이 아니라, 각 계층이 왜 필요하고 없으면 무엇이 안 되는지를
직접 확인하며 올라가는 것이 이 저장소의 목표다.

### 적용 사례를 구체적으로 잡은 이유

산업 현장에서 흔히 발생할 수 있는 "픽앤플레이스"를 시뮬레이션 대상으로 하고 여기에 
실제 PCB와 같은 기판을 정방향을 구분하여 지그에 안착시키는 것을 목표로 삼았다.
여기에 더해서 다품종 소량 생산이라는 상황을 가정하여 PCB, 지그 형태 등이 바뀔 수 
있다는 전제하여 AI를 활용하여 로봇의 파라미터 변경없이 변화되는 작업 환경에 대응하도록 
하는 것이 가능한지까지 해보려고 한다.

로봇은 UR5e로 시작했다. 이 작업 스케일(기판 2~3cm, 지그 15cm 남짓)에는 과한 사양이라
`ur_type` 인자 하나로 UR3e 등 더 작은 모델로 바꿀 수 있게 해 두었다
(`src/arm_description/urdf/ur5e.urdf.xacro`).

## 언더레이 / 오버레이
작업 공간은 아래와 같이 언더레이와 오버레이의 구조를 가진다.

| | 내용 | 위치 |
|---|---|---|
| 언더레이 | UR 드라이버, MoveIt 2, ros2_control 등 업스트림 소스 | 이 저장소 **밖** `../ur_ws` |
| 오버레이 | 직접 작성한 패키지 3개 | 이 저장소 `src/` |

언더레이는 Universal Robots의 ROS 패키지 소스이다. 이 패키지 소스를 활용하여 내가 작성한
코드를 오버레이 형태로 사용한다.
이 저장소는 **오버레이 워크스페이스 하나**다. 즉, 직접 작성한 코드만 들어 있다.

업스트림에서 그대로 가져다 쓸 수 있는 것(로봇 형상 매크로, SRDF의 충돌쌍 목록 등)은
`$(find ur_description)` 식으로 include해서 재사용하고, 직접 결정해야 하는 것
(`<ros2_control>` 태그, 컨트롤러 yaml, 그리퍼 형상, 응용 로직)만 새로 작성했다.

언더레이가 어떤 저장소의 어떤 브랜치를 쓰는지를 `environment/ur_ws.repos`에 선언해 두었다.

## 구조

호스트에서의 디렉토리 이름은 상관없다. `jazzy.sh`가 스크립트 위치를 기준으로 경로를
잡고, 컨테이너 안에서는 항상 `~/arm_study_ws`로 마운트된다.

```
robot-arm-study/                     ← 이 저장소 (오버레이). 컨테이너 안 ~/arm_study_ws
├── environment/                     컨테이너 환경
│   ├── Containerfile                ROS 2 Jazzy + Gazebo + MoveIt + VirtualGL
│   ├── build.sh                     이미지 빌드 (호스트 계정/UID로 자동 설정)
│   ├── jazzy.sh                     컨테이너 기동 및 접속
│   ├── ur_ws.repos                  언더레이 소스 목록 (vcs import 용)
│   └── .containerignore
├── src/
│   ├── arm_description/             형상 — URDF/xacro, ros2_control 선언, SRDF
│   ├── arm_bringup/                 실행 — launch, 컨트롤러/MoveIt 설정
│   └── arm_control_app/             응용 — C++ MoveGroupInterface 노드
└── docs/                            디버깅 기록
```

## 시작하기

### 1. 언더레이 소스 준비 (UR Robot ROS2 패키지 소스. 바이너리 패키지가 아닌 git 저장소 복제)

언더레이는 저장소 밖, 이 저장소와 같은 디렉토리 계층에 둔다. 이미지 빌드 시 `rosdep`으로
의존성 패키지를 미리 설치해 두는 데 필요하다.

받아야 할 저장소가 14개라 하나씩 `git clone` 하지 않고 `vcs`(vcstool)로 한 번에
가져온다. 어떤 저장소의 어떤 브랜치를 받을지는 `environment/ur_ws.repos`에 적혀 있다.

```bash
sudo apt install python3-vcstool     # vcs 가 없다면

cd ..                                # 이 저장소의 부모 디렉토리
mkdir -p ur_ws/src && cd ur_ws
vcs import src < ../robot-arm-study/environment/ur_ws.repos
```

받아지는 것은 크게 세 갈래다.

| 갈래 | 저장소 |
|---|---|
| UR 로봇 | `ur_driver`, `..._ROS2_Description`, `..._Client_Library`, `ur_simulation_gz`, `ur_msgs` |
| 모션 플래닝 | `moveit2`, `moveit_msgs`, `srdfdom` |
| 제어 | `ros2_control`, `ros2_controllers`, `control_msgs`, `control_toolbox`, `realtime_tools`, `kinematics_interface` |

바이너리 대신 소스로 받는 이유는, 업스트림 코드를 직접 열어 보며 배우는 것이 목적이고
`ur_description`의 형상 매크로나 `ur_moveit_config`의 설정을 이 저장소에서 include해
재사용하기 때문이다. `docs/`의 디버깅 기록도 이 소스를 읽어 원인을 찾은 것이다.

> **주의** — `Containerfile`에서 이미지를 생성할 때 UR 바이너리 패키지를 apt로 설치하지 않아야 한다.
> 특히 **`ur-description`, `ur-moveit-config`, `ur-msgs`, `ur-robot-driver`는
> 바이너리를 설치하면 안 된다.** 소스로 빌드한 뒤 UR 시뮬레이션 런치를 실행하면 충돌이 발생한다.
> `Containerfile`에도 같은 경고를 주석으로 남겨 두었다.

### 2. 컨테이너 이미지 빌드

```bash
cd robot-arm-study/environment
./build.sh
```

`build.sh`는 현재 호스트 계정명과 UID/GID를 그대로 이미지에 넣는다. 그래야
컨테이너에서 만든 파일이 호스트에서도 본인 소유로 보이고, 마운트한 워크스페이스를
호스트 에디터로 그대로 편집할 수 있다.

컨테이너에는 3D 그래픽 가속기를 사용할 수 있도록 vglrun을 미리 설치해 두었다. 
3차원 가속을 컨테이너에서도 사용할 수 있게 하려면 podman 실행 옵션에 자신의 환경에 맞게 
그래픽 카드 관련 설정을 해주어야 한다.

### 3. 컨테이너 기동

```bash
./jazzy.sh
```

마운트 구조 (경로는 스크립트 위치 기준으로 자동 산출):

| 호스트 | 컨테이너 |
|---|---|
| `robot-arm-study/` | `~/arm_study_ws` |
| `../ur_ws/` | `~/ur_ws` |

> **VS Code 로 작업하는 경우** — `./jazzy.sh` 대신 저장소를 VS Code 로 열고
> `Reopen in Container` 를 실행한다. `.devcontainer/devcontainer.json` 이
> `jazzy.sh` 와 동일한 마운트 구조(`~/arm_study_ws`, `~/ur_ws`)를 만들도록
> 맞춰 두었다. 어느 쪽으로 들어가든 컨테이너 안 경로가 같아야 `install/` 에
> 기록되는 경로가 어긋나지 않는다.

### 4. 빌드 (컨테이너 안에서)

언더레이 먼저, 오버레이는 그 다음. 소싱 순서가 곧 의존 순서다.

```bash
cd ~/ur_ws && colcon build            # 최초 1회. 시간이 오래 걸린다
source install/setup.bash

cd ~/arm_study_ws && colcon build --symlink-install
source install/setup.bash
```

`--symlink-install` 옵션으로 설치하기 때문에 launch/yaml/xacro 수정은 다시 빌드할 
필요없이 곧바로 반영된다.

### 5. 실행 (터미널 3개)

```bash
# 1) Gazebo + 컨트롤러
ros2 launch arm_bringup arm_study_bringup.launch.py

# 2) MoveIt + RViz
ros2 launch arm_bringup move_group.launch.py

# 3) 응용 노드 — 코드에서 "home" 자세로 이동
ros2 run arm_control_app arm_control_app
```

## 진행 단계

학습 순서대로 쌓았다. 각 단계는 "그 전 단계까지로는 안 되는 것"을 하나씩 해결한다.

| 단계 | 한 일 | 상태 |
|---|---|---|
| 1. 컨테이너 환경 | Podman + ROS 2 Jazzy 이미지, GPU 렌더링(VirtualGL), 언더레이 소스 빌드 | 완료 |
| 2. 패키지 골격과 install 규칙 | `arm_description`/`arm_bringup` 생성. `install(DIRECTORY ...)`를 넣어야 `$(find ...)`가 해소된다는 것을 `xacro` 명령으로 확인 | 완료 |
| 3. URDF 작성과 Gazebo 스폰 | `ur_description`의 `ur_macro.xacro` 재사용, `world` 링크에 고정. 컨트롤러 없이 스폰하면 **관절이 중력에 늘어져 쓰러지는 것**을 확인 — 제어기 없는 로봇은 그냥 관절 달린 물체다 | 완료 |
| 4. ros2_control 선언 | `<ros2_control>` 태그 + `gz_ros2_control` 플러그인. 관절 인터페이스는 `ur_joint_control.xacro`를 include해 재사용 | 완료 |
| 5. 컨트롤러 설정과 활성화 | `arm_controllers.yaml`에 `joint_state_broadcaster` + `joint_trajectory_controller`. 스포너로 활성화한 뒤 `ros2 control list_controllers`로 상태 확인. 명령줄에서 궤적을 직접 주입해 팔이 실제로 움직이는 것까지 확인 | 완료 |
| 6. SRDF와 MoveIt 연동 | SRDF 작성(충돌쌍 목록은 `ur_moveit_config`에서 재사용), `move_group` 런치. RViz의 Plan/Execute로 Gazebo 팔 제어 | 완료 |
| 7. C++ 응용 노드 | `MoveGroupInterface`로 `setNamedTarget("home")` → `setPoseTarget`. RViz 없이 코드에서 직접 제어 | 완료 |
| 8. 그리퍼 | `tool0`에 평행 그리퍼(prismatic 2축) 형상 추가 | **진행 중** |
| 9. 픽업 사이클 | 공급 트레이 → 지그 → 배출 트레이를 순회하며 실제 pick/place 수행 | 예정 |
| 10. 정방향 판별과 분기 | 기판 방향이 맞는지 판단해 맞으면 지그에 안착, 틀리면 돌려서 재시도. 결과에 따라 갈라지는 흐름이라 상태 머신으로 처리 | 예정 |
| 11. 비전 | 카메라로 기판의 위치와 방향 인식. ArUco 마커로 시작해 추후 YOLO로 교체 가능하게 설계 | 예정 |
| 12. AI 적용 | 기판·지그 형태가 바뀌어도 로봇 파라미터를 고치지 않고 대응 가능한지 확인 | 예정 |

git 저장소는 8단계 도중에 만들었다. 그래서 1~7단계의 결과물은 최초 커밋에 한꺼번에
들어가 있고, 그 이후부터 단계별로 커밋이 나뉜다.

## 라이센스

[BSD 3-Clause](LICENSE). 이 저장소가 include해 쓰는 `ur_description`, MoveIt 2 등이
같은 라이센스를 쓴다. 언더레이(`ur_ws`)의 업스트림 소스는 이 저장소에 포함되지 않으며
각자의 라이센스를 따른다.

## 디버깅 기록

과정에서 겪은 문제와 원인 분석은 [`docs/`](docs/)에 정리했다.

- [**MoveIt은 움직이는데 Gazebo는 안 움직이는 문제**](docs/moveit-gazebo-controller-mismatch.md)
  — 언더레이(`ur_ws`)에서 UR 공식 예제를 돌려 보다 겪은 문제. MoveIt이 명령을 보내는
  컨트롤러 이름(실물 로봇용 `scaled_joint_trajectory_controller`)과 시뮬레이터에 실제로
  떠 있는 이름(`joint_trajectory_controller`)이 달라 명령이 도달하지 못했다.

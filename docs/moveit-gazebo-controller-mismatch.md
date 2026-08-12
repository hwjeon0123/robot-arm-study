# MoveIt은 움직이는데 Gazebo는 안 움직이는 문제

UR5e 시뮬레이션 구축 중 발생한 장애의 진단 기록.

**요약** — MoveIt이 A라는 이름의 컨트롤러에게 명령을 보냈는데, Gazebo 쪽에는 B라는 이름만 떠 있었다. 이름이 달라 명령이 도달하지 못했다.

---

## 0. 실행 환경

Podman 컨테이너 안의 ROS 2 Jazzy. 워크스페이스는 `~/ur_ws`.

```bash
source /opt/ros/jazzy/setup.bash
source ~/ur_ws/install/setup.bash

# 문제가 발생한 런치 (Gazebo + ros2_control + MoveIt + RViz 통합)
ros2 launch ur_simulation_gz ur_sim_moveit.launch.py ur_type:=ur5e
```

이 런치 하나가 내부적으로 두 개를 포함한다.

- `ur_sim_control.launch.py` — Gazebo 띄우고, 로봇 스폰하고, **컨트롤러 기동**
- `ur_moveit.launch.py` — **move_group**과 RViz 기동

**즉 컨트롤러를 띄우는 쪽과 MoveIt을 띄우는 쪽이 서로 다른 패키지다.** 이번 문제의 근본 배경이 여기에 있다.

> MoveIt 없이 Gazebo와 컨트롤러만 띄우려면:
> `ros2 launch ur_simulation_gz ur_sim_control.launch.py ur_type:=ur5e`

---

## 1. 문제 정의

### 명령이 흐르는 경로

```
MoveIt  ──[FollowJointTrajectory]──▶  JTC  ──▶  gz_ros2_control  ──▶  Gazebo
 (계산)                                (실행)         (연결)            (물리)
```

- **MoveIt** — 궤적을 **계산만** 한다. 로봇을 직접 못 움직인다. 결과를 JTC에게 넘겨야 한다.
- **JTC** (joint_trajectory_controller) — 궤적을 받아 관절을 실제로 움직인다.
- **RViz** — 뷰어일 뿐, 아무것도 제어하지 않는다.

### 증상

RViz와 Gazebo가 정상적으로 뜬다. Plan → Execute 하면 **RViz 속 팔은 움직이는데 Gazebo 속 팔은 안 움직인다.**

### 증상의 해석

RViz의 `Planned Path`는 계산 결과를 재생하는 애니메이션이다. **실행 성공 여부와 무관하게 재생된다.** 계획서를 그려본 것이지 로봇이 움직인 기록이 아니다.

→ 계산은 성공했고, 계산 결과가 JTC에게 전달되지 못했다. 위 그림의 **첫 화살표**가 끊겼다.

---

## 2. 문제 확인

### 확인 1 — 액션 목록 (함정)

```
$ ros2 action list | grep follow_joint_trajectory
/joint_trajectory_controller/follow_joint_trajectory
/scaled_joint_trajectory_controller/follow_joint_trajectory
```

둘 다 보이지만 **이건 판정 근거가 못 된다.** 이 명령은 토픽 흔적을 스캔하는데, 흔적은 명령을 받는 쪽(서버)뿐 아니라 거는 쪽(클라이언트)도 남긴다. 서버가 없어도 목록에 뜬다.

### 확인 2 — 컨트롤러 목록 (판정 근거)

```
$ ros2 control list_controllers
joint_trajectory_controller  ...  active
joint_state_broadcaster      ...  active
```

이 명령은 controller_manager에 직접 묻는다. **`scaled_joint_trajectory_controller`는 없다.** MoveIt은 존재하지 않는 컨트롤러를 부르고 있었다.

### 확인 3 — 하부 격리 시험

MoveIt을 빼고 JTC에 직접 궤적을 던졌다.

```bash
ros2 topic pub -1 /joint_trajectory_controller/joint_trajectory \
  trajectory_msgs/msg/JointTrajectory "{
  joint_names: [shoulder_pan_joint, shoulder_lift_joint, elbow_joint,
                wrist_1_joint, wrist_2_joint, wrist_3_joint],
  points: [{positions: [0.0, -1.57, 1.57, -1.57, -1.57, 0.0],
            time_from_start: {sec: 3}}]
}"
```

**결과: Gazebo의 팔이 움직였다.** → `JTC 이후` 구간은 전부 정상. 결함은 첫 화살표 하나로 확정.

---

## 3. 해결 방안 모색

### 왜 어긋났는가

`ur_moveit_config`는 원래 **실물 UR 로봇용** 패키지다. 실물 드라이버는 UR 티치펜던트의 속도 노브에 연동되는 `scaled_joint_trajectory_controller`를 띄우므로, 기본값이 그쪽으로 잡혀 있다.

반면 `ur_simulation_gz`에는 그 컨트롤러가 **정의조차 없다.** 평범한 `joint_trajectory_controller` 하나뿐이다.

→ 실물용 기본값과 시뮬레이터용 기본값이 맞물리지 않았다.

### 방안 비교

| 방안 | 내용 | 판단 |
|---|---|---|
| **A** | MoveIt의 기본 컨트롤러 선택을 바꾼다 | 즉시 적용. 단 업스트림 파일이라 `git pull` 시 소실 |
| **B** | `arm_bringup`에서 파라미터를 덮어쓴다 | 업스트림 무손상. 런치 파일 작성 필요 |
| **C** | 시뮬레이터에 scaled JTC를 추가로 띄운다 | **기각.** 실물 전용 인터페이스에 의존해 로드 실패 예상 |

**채택: A로 원인을 확정한 뒤 B로 정착.** 확정도 안 된 가설을 위해 런치 파일부터 쓰는 것은 순서가 뒤집힌 것이다.

---

## 4. 적용 후 검증

### 적용

파일: `~/ur_ws/src/ur_driver/ur_moveit_config/config/moveit_controllers.yaml`

```yaml
  scaled_joint_trajectory_controller:
    default: true        ← false 로

  joint_trajectory_controller:
    default: false       ← true 로
```

두 값을 맞바꾼다. 다른 줄은 건드리지 않는다.

**리빌드 불필요.** 이 워크스페이스는 `--symlink-install`로 빌드되어 설치본이 소스를 가리키는 심볼릭 링크다. 소스를 고치면 즉시 반영된다. 런치만 다시 띄우면 된다.

### 검증

1. 실행 중인 런치를 전부 종료(`Ctrl+C`)하고 다시 띄운다.

   ```bash
   ros2 launch ur_simulation_gz ur_sim_moveit.launch.py ur_type:=ur5e
   ```

2. `move_group` 로그에서 `Action client not connected` 경고가 **사라졌는지** 확인
   - `Added FollowJointTrajectory controller for ...` 가 두 줄 나오는 건 정상 (등록은 둘 다 되고, 바뀐 건 **선택**이다)
3. RViz에서 Plan → Execute

### 판정

| 결과 | 판정 |
|---|---|
| Gazebo의 팔이 움직인다 | 해결 |
| 여전히 안 움직인다 | 진단 오류. 2장으로 복귀 |
| 움직이다 `ABORTED`로 끝난다 | `ur_controllers.yaml`의 `constraints`가 실물 기준이라 빡빡한 경우. **이 증상이 실제로 나오기 전엔 손대지 않는다** |

### 결과 (2026-08-08)

**해결.** `default:` 두 줄을 맞바꾸고 런치를 재실행한 뒤, RViz 모션 플래닝에서 Plan → Execute 하자 **Gazebo의 팔이 함께 움직였다.**

- 리빌드 없이 런치 재실행만으로 반영됐다. `--symlink-install` 전제가 맞았음이 확인됐다.
- 4-4에서 우려했던 `ABORTED` / goal tolerance 문제는 발생하지 않았다. `ur_controllers.yaml`의 `constraints`는 손대지 않는다.
- 이로써 `MoveIt → JTC → gz_ros2_control → Gazebo` 전 구간이 연결됐다. 인프라 계층 종료.

---

## 5. 총 정리

| 항목 | 내용 |
|---|---|
| 증상 | RViz는 움직이고 Gazebo는 안 움직임 |
| 실제 원인 | MoveIt이 부르는 컨트롤러 이름과 실존 이름의 불일치 |
| 근본 이유 | 실물 로봇용 기본 설정을 시뮬레이터에 그대로 사용 |
| 조치 | MoveIt의 기본 컨트롤러를 `joint_trajectory_controller`로 변경 |

### 재사용 가능한 원칙

**① 화면의 움직임은 제어의 성공이 아니다.**
뷰어(RViz)와 실행 계층(컨트롤러)은 분리되어 있다. 뷰어가 뭔가를 그렸다는 건 실행 성공의 증거가 못 된다.

**② 계층을 나눠 아래부터 격리 시험한다.**
상위(MoveIt)를 통째로 빼고 하위(JTC)에 직접 명령을 주입했다. 하위가 정상임이 증명되자 문제 범위가 화살표 하나로 줄었다. **이번 진단의 결정타.**

**③ 도구가 무엇을 근거로 답하는지 알고 써야 한다.**

| 명령 | 근거 | 판정 사용 |
|---|---|---|
| `ros2 action list` | 토픽 흔적 스캔 | ✗ 클라이언트만 있어도 뜸 |
| `ros2 control list_controllers` | controller_manager 직접 질의 | ✓ |
| `ros2 action info <액션>` | 서버/클라이언트 분리 집계 | ✓ 확인 사살용 |

같은 대상을 보는 세 명령이 서로 다른 답을 낸다. 근거를 모르고 쓰면 잘못된 결론에 도달한다.

### 남은 부채

**방안 B가 아직 적용되지 않았다.** 현재 수정은 업스트림 파일(`ur_moveit_config`)을 직접 고친 상태이므로, `git pull` 하면 사라지고 증상이 재발한다. `arm_bringup`에서 파라미터를 덮어쓰는 구조로 옮겨야 정착된다.

### 다음 단계

Python 노드로 목표 좌표 전달 → Gazebo에 트레이·지그 도형 스폰 및 충돌 객체 등록 → 상태 머신으로 예외 처리 흐름 구성.

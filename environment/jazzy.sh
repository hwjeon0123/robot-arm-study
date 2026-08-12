#!/bin/bash
# ==============================================================================
# ROS 2 Jazzy 컨테이너 기동 / 접속
#
#   마운트 구조 (이 스크립트 위치 기준으로 자동 산출)
#     <저장소>/                → 컨테이너 /home/${CTR_USERNAME}/arm_study_ws   오버레이
#     <저장소>/../ur_ws        → 컨테이너 /home/${CTR_USERNAME}/ur_ws          언더레이
#
#   환경 변수로 덮어쓸 수 있다:
#     CTR_USERNAME   컨테이너 안 계정명. 이미지 빌드 시
#                    --build-arg USERNAME=... 로 준 값과 반드시 일치해야 한다.
#     UR_WS          언더레이를 다른 곳에 두었을 때 그 경로를 지정
#     IMAGE_NAME     사용할 이미지 (기본: build.sh 가 생성하는 이미지 이름)
#     CTR_NAME       컨테이너 이름. 같은 이미지로 여러 개를 띄울 때 구분용
# ==============================================================================

IMAGE_NAME="${IMAGE_NAME:-localhost/robot-arm-study:jazzy}"
CTR_NAME="${CTR_NAME:-robot-arm-study}"

# 컨테이너 내부의 사용자 계정 = 이미지를 빌드한 호스트 시스템의 사용자.
# (build.sh 실행 때 id 명령으로 호스트 계정과 UID/GID 를 읽어 --build-arg 로 넘긴다)
# 홈 경로 /home/<계정> 아래로 워크스페이스를 마운트하므로 값이 정확해야 한다.
# 추측하지 않고 이미지에 기록된 값을 그대로 읽는다. 이미지가 아직 없으면
# 현재 사용자로 폴백한다 (build.sh 를 쓰면 결국 같은 값이 된다).
if [ -z "${CTR_USERNAME}" ]; then
    CTR_USERNAME="$(podman image inspect "${IMAGE_NAME}" \
                        --format '{{.Config.User}}' 2>/dev/null)"
    CTR_USERNAME="${CTR_USERNAME:-$(id -un)}"
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARM_WS="$(dirname "${SCRIPT_DIR}")"              # 이 저장소 = 오버레이 워크스페이스
UR_WS="${UR_WS:-$(dirname "${ARM_WS}")/ur_ws}"   # 언더레이 (저장소 밖, 업스트림 소스)

if [ ! -d "${UR_WS}/src" ]; then
    echo "[오류] 언더레이를 찾을 수 없습니다: ${UR_WS}/src"
    echo "       environment/README.md 의 언더레이 준비 절차를 먼저 수행하거나,"
    echo "       UR_WS=<경로> ./jazzy.sh 로 위치를 지정하세요."
    exit 1
fi

# 1. 해당 이미지를 사용하는 '실행 중인' 컨테이너 ID 확인
CONTAINER_ID=$(podman ps -q -f ancestor=$IMAGE_NAME)

echo "Container ID is ${CONTAINER_ID}"

# 아래는 예전 명령어 
PODMAN_RUN='podman run -it --rm --net=host -e DISPLAY=$DISPLAY \
		-v /tmp/.X11-unix:/tmp/.X11-unix -c "source /opt/ros/jazzy/setup.bash" \
		--security-opt label=type:container_runtime_t --userns=keep-id \
		localhost/ros2-jazzy /bin/bash'

# bash 실행 후 다른 프로그램을 실행하려면
# /bin/bash -c "source /opt/ros/jazzy/setup.bash && {실행할 프로그램}" 
# 이런 형식으로 명령문을 넣으면 됨

PODMAN_RUN_OPTS=(
		-it
		--rm 
		--name "${CTR_NAME}"
		--net=host 
		-e DISPLAY=$DISPLAY 
		-v /tmp/.X11-unix:/tmp/.X11-unix
		--security-opt label=type:container_runtime_t 
		--userns=keep-id --user $(id -u):$(id -g)
		--group-add keep-groups
		# GPU 인덱스는 환경에 맞게 바꿀 것. 1장뿐이면 gpu=0 또는 gpu=all.
		# 이 환경은 2장을 용도별로 나눠 쓴다 —
		#   0번(고성능) : YOLO 등 로컬 AI 용으로 비워 둠
		#   1번(저성능) : 시뮬레이션 렌더링은 이 정도로 충분
		--device nvidia.com/gpu=1
		-e NVIDIA_DRIVER_CAPABILITIES=graphics,display,utility
		-e VGL_DISPLAY=egl
		# 지정하지 않으면 Qt 앱(RViz)이 실행할 때마다
		#   "QStandardPaths: XDG_RUNTIME_DIR not set" 경고를 낸다.
		# 동작에는 지장이 없지만 로그가 지저분해지므로 명시한다.
		-e XDG_RUNTIME_DIR=/tmp/runtime-${CTR_USERNAME}
	    -v "${UR_WS}:/home/${CTR_USERNAME}/ur_ws:rw,z"
	    -v "${ARM_WS}:/home/${CTR_USERNAME}/arm_study_ws:rw,z"
	)

echo "Podman opt is ${PODMAN_RUN_OPTS[@]}"

if [ -n "$CONTAINER_ID" ]; then
    echo "실행 중인 컨테이너($CONTAINER_ID)를 발견했습니다. 접속합니다..."
    podman exec -it $CONTAINER_ID /bin/bash

# 2. 실행 중은 아니지만 '존재하는' 컨테이너가 있는지 확인
else
    STOPPED_ID=$(podman ps -aq -f ancestor=$IMAGE_NAME | head -n 1)
    
    if [ -n "$STOPPED_ID" ]; then
        echo "중지된 컨테이너($STOPPED_ID)를 다시 시작합니다..."
        podman start -ai $STOPPED_ID
    else
        echo "새로운 컨테이너를 생성합니다..."
        podman run -d "${PODMAN_RUN_OPTS[@]}" $IMAGE_NAME /bin/bash
    fi
fi


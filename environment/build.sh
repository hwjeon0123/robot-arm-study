#!/bin/bash
# ==============================================================================
# ROS 2 Jazzy 개발 이미지 빌드
#
#   컨테이너 안 계정을 "지금 이 스크립트를 실행하는 호스트 사용자" 와 같게 만든다.
#   UID/GID 까지 맞추므로, --userns=keep-id 로 컨테이너를 띄우면 컨테이너에서
#   만든 파일이 호스트에서도 본인 소유로 보인다. (마운트한 워크스페이스를
#   호스트 에디터로 그대로 편집할 수 있는 이유)
#
#   빌드 컨텍스트가 이 저장소가 아니라 그 "부모 디렉토리" 인 점에 주의한다.
#   Containerfile 이 rosdep 스테이징을 위해 ./ur_ws/src 를 COPY 하는데,
#   ur_ws(언더레이)는 업스트림 소스라 이 저장소에 포함하지 않기 때문이다.
#
#   사용법:
#     ./build.sh                해당 사용자 계정으로 이미지 빌드
#     UR_WS=<경로> ./build.sh   언더레이가 다른 곳에 있을 때
# ==============================================================================
set -e

IMAGE_NAME="${IMAGE_NAME:-localhost/robot-arm-study:jazzy}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARM_WS="$(dirname "${SCRIPT_DIR}")"
UR_WS="${UR_WS:-$(dirname "${ARM_WS}")/ur_ws}"

# Containerfile 의 COPY 가 './ur_ws/src' 를 참조하므로,
# 컨텍스트는 ur_ws 의 부모이고 ur_ws 의 디렉토리명은 그대로여야 한다.
BUILD_CTX="$(dirname "${UR_WS}")"

if [ ! -d "${UR_WS}/src" ]; then
    echo "[오류] 언더레이 소스를 찾을 수 없습니다: ${UR_WS}/src"
    echo "       environment/README.md 의 언더레이 준비 절차를 먼저 수행하세요."
    exit 1
fi

if [ "$(basename "${UR_WS}")" != "ur_ws" ]; then
    echo "[오류] 언더레이 디렉토리명은 'ur_ws' 여야 합니다 (현재: $(basename "${UR_WS}"))."
    echo "       Containerfile 의 COPY 경로가 이 이름에 묶여 있습니다."
    exit 1
fi

echo "[build] 이미지        : ${IMAGE_NAME}"
echo "[build] 계정          : $(id -un) (uid=$(id -u), gid=$(id -g))"
echo "[build] 빌드 컨텍스트 : ${BUILD_CTX}"

podman build \
    -f "${SCRIPT_DIR}/Containerfile" \
    --ignorefile "${SCRIPT_DIR}/.containerignore" \
    --build-arg USERNAME="$(id -un)" \
    --build-arg USER_UID="$(id -u)" \
    --build-arg USER_GID="$(id -g)" \
    -t "${IMAGE_NAME}" \
    "${BUILD_CTX}"

echo ""
echo "[build] 완료. 컨테이너 기동:  ./jazzy.sh"

#!/usr/bin/env python3
"""ArUco 마커 텍스처 생성기.

Gazebo 모델에 붙일 마커 이미지를 만든다. 빌드나 실행에는 쓰이지 않고,
마커를 새로 만들거나 ID/사전을 바꿀 때만 수동으로 실행한다.

컨테이너 안에서 실행할 것 (OpenCV 는 컨테이너에만 설치돼 있다):
    python3 tools/gen_aruco.py --id 0 --out <경로>/aruco_4x4_id0.png

주의 — OpenCV 4.6 (Ubuntu 24.04 기본) 기준 API 다.
4.7 부터는 Dictionary_get -> getPredefinedDictionary,
drawMarker -> generateImageMarker 로 이름이 바뀌었다.
"""

import argparse

import cv2
import cv2.aruco as aruco

# DICT_4X4_50 : 데이터 격자 4x4, 서로 다른 마커 50 종.
# 격자가 작을수록 같은 물리 크기에서 한 칸이 커져 멀리서도 잘 검출된다.
# 시뮬레이션 카메라 해상도가 넉넉하지 않으므로 4x4 를 쓴다.
DICTIONARY = aruco.DICT_4X4_50

# drawMarker 는 데이터 격자(4x4) 바깥에 검은 테두리를 한 칸 둘러 출력하므로
# 실제로 그려지는 것은 6x6 격자다. 360 = 60px x 6칸.
MARKER_PX = 360

# 흰 여백(quiet zone). 검출기는 검은 테두리와 배경의 경계로 마커를 찾기 때문에
# 여백이 없으면 검출되지 않는다. 최소 한 칸 폭이 필요하므로 60px.
QUIET_PX = 60


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--id", type=int, default=0, help="사전 내 마커 ID")
    parser.add_argument("--out", required=True, help="저장할 PNG 경로")
    args = parser.parse_args()

    dictionary = aruco.Dictionary_get(DICTIONARY)
    marker = aruco.drawMarker(dictionary, args.id, MARKER_PX)

    image = cv2.copyMakeBorder(
        marker, QUIET_PX, QUIET_PX, QUIET_PX, QUIET_PX,
        cv2.BORDER_CONSTANT, value=255,
    )
    cv2.imwrite(args.out, image)

    # 저장한 이미지를 되읽어 실제로 검출되는지 확인한다.
    reloaded = cv2.imread(args.out, cv2.IMREAD_GRAYSCALE)
    _, ids, _ = aruco.detectMarkers(
        reloaded, dictionary, parameters=aruco.DetectorParameters_create()
    )
    detected = None if ids is None else ids.ravel().tolist()

    print(f"저장: {args.out}  {image.shape[1]}x{image.shape[0]}px")
    print(f"  마커 본체 {MARKER_PX}px + 여백 {QUIET_PX}px x 2")
    print(f"  본체 비율 {MARKER_PX / image.shape[0]:.3f}"
          f"  (자세 추정에 넘길 물리 크기 = 판 크기 x 이 값)")
    print(f"검출 확인: {detected}")


if __name__ == "__main__":
    main()

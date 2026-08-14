# 커밋 메시지 규칙

[Conventional Commits](https://www.conventionalcommits.org/)를 따른다.
혼자 쓰는 학습 저장소지만, velog 글에서 커밋을 해시로 고정해 인용하기 때문에
`git log --oneline` 이 목차처럼 읽히는 것이 중요하다.

## 형식

```
<type>(<scope>): <요약>
                                  ← 빈 줄 (없으면 git 이 본문을 제목으로 취급한다)
<이 변경이 왜 필요한지 1~3문장>

1) 항목
   - 세부
     - 더 세부
2) 항목
```

- **제목은 72자 이내.** `git log --oneline` 과 GitHub 목록에는 이 줄만 나온다
- **제목 끝에 마침표를 찍지 않는다**
- 본문은 72자에서 줄바꿈한다. git 은 자동 줄바꿈을 하지 않아 터미널에서 깨진다
- 커밋 메시지는 **마크다운으로 렌더링되지 않는다.** GitHub 도 고정폭으로 그대로
  보여주므로, 들여쓰기로 계층을 표현한다

## type

| type | 쓰는 경우 |
|---|---|
| `feat` | 없던 기능·동작을 추가 |
| `fix` | 잘못 동작하던 것을 고침 |
| `docs` | 문서만 변경 |
| `refactor` | 동작은 그대로인데 구조·이름을 정리 |
| `chore` | 빌드 산출물 제외, 파일 정리 등 잡일 |
| `build` | 이미지·의존성 등 빌드 방식 변경 |
| `test` | 테스트 추가·수정 |

## scope

| scope | 대상 |
|---|---|
| `container` | `environment/Containerfile`, 이미지 |
| `scripts` | `environment/*.sh` |
| `devcontainer` | `.devcontainer/` |
| `description` | `src/arm_description` |
| `bringup` | `src/arm_bringup` |
| `control-app` | `src/arm_control_app` |
| `readme` | `README.md` |
| `docs` | `docs/` |

여러 scope 에 걸치면 생략하거나 `env` 처럼 묶어 쓴다.

## 예

```
feat(container): 워크스페이스 자동 소싱과 시작 위치 설정

컨테이너를 --rm 으로 띄우므로 컨테이너 안에서 .bashrc 를 고쳐도 종료할 때
사라진다. 소싱 설정이 있어야 할 자리는 컨테이너가 아니라 이미지다.

1) 워크스페이스 자동 소싱을 .bashrc 에 추가
   - 언더레이(ur_ws) -> 오버레이(robot-arm-study) 순서
   - 최초 colcon 빌드 전에는 install/ 이 없으므로 파일 존재 검사를 붙임
2) WORKDIR 을 ur_ws 에서 robot-arm-study 로 변경
```

## 커밋 단위

**작업 단위로 그때그때 커밋한다.** 여러 관심사를 한 번에 바꿔 놓고 나중에 나누려 하면,
같은 파일에 관심사가 섞여 있어 `git add -p` 없이는 쪼갤 수 없다.

관심사가 이미 섞여 버렸다면 억지로 나누지 말고, 하나의 커밋으로 두되 본문에
항목을 나눠 적는다.

## 참고 — velog 인용

글에서 코드를 가리킬 때는 브랜치가 아니라 **커밋 해시**를 쓴다.

```
https://github.com/hwjeon0123/robot-arm-study/blob/<해시>/environment/Containerfile
https://github.com/hwjeon0123/robot-arm-study/blob/<해시>/environment/Containerfile#L103-L115
```

`blob/main/...` 은 파일이 바뀌면 글과 어긋나고, 행 번호는 몇 줄만 추가돼도 엉뚱한 곳을
가리킨다. GitHub 에서 파일을 연 상태로 `y` 를 누르면 주소의 브랜치명이 해시로 바뀐다.

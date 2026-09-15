# 커밋 메시지 규칙

[Conventional Commits](https://www.conventionalcommits.org/)를 따른다.

## 형식

```
<type>(<scope>): <요약>

- 항목
- 항목
```

- 제목은 72자 이내, 끝에 마침표를 찍지 않는다
- 제목 다음에 빈 줄. 없으면 git 이 본문까지 제목으로 취급한다
- 본문은 불릿 2~4개. 문단 설명은 넣지 않는다
- 마크다운이 렌더링되지 않으므로 계층은 들여쓰기로 표현한다

## type

| type | 쓰는 경우 |
|---|---|
| `feat` | 없던 기능·동작을 추가 |
| `fix` | 잘못 동작하던 것을 고침 |
| `docs` | 문서만 변경 |
| `refactor` | 동작은 그대로인데 구조·이름을 정리 |
| `chore` | 파일 정리 등 잡일 |
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
| `vision` | `src/arm_vision` |
| `readme` | `README.md` |
| `docs` | `docs/` |

여러 scope 에 걸치면 생략하거나 `env` 로 묶는다.

## 예

```
feat(container): 워크스페이스 자동 소싱, WORKDIR 변경

- .bashrc 에 언더레이 -> 오버레이 순서로 ROS2 환경 변수 소싱 명령 추가
- WORKDIR 을 ur_ws -> robot-arm-study 로 변경
- 필요없는 주석 정리
```

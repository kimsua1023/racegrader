# RaceGrader

> xv6 동시성 버그(락 누락, race condition) 탐지 오토그레이더 — "한 번 통과가 곧 정답은 아니다"

[![CI](https://github.com/kimsua1023/racegrader/actions/workflows/ci.yml/badge.svg)](https://github.com/kimsua1023/racegrader/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)

## 문제의식

기존 오토그레이더는 출력값 비교 방식이라, 커널 과제처럼 동시 접근 안전성이 핵심인
과제를 제대로 채점하지 못합니다. 락 누락이나 레이스 컨디션은 특정 타이밍에서만
드러나는 flaky한 특성이 있어, 한 번 통과가 정답을 보장하지 않습니다.

## RaceGrader가 하는 일

xv6 같은 교육용 커널 과제를 대상으로 스케줄링 타이밍을 매번 다르게 흔들며 반복
실행해, 겉보기엔 통과했지만 실제로는 위험한 코드를 찾아냅니다. panic/assert,
상태 불변식 위반을 관찰해 실패를 판단하고, 몇 번 중 몇 번 실패했는지와 의심되는
코드 위치를 알려주는 리포트를 생성합니다.

## 프로젝트 구조

```
.
├── kernel/     # xv6 fork + 결함 시나리오 + 스케줄링 교란 훅 (김수아)
├── cli/        # Go CLI + Bubble Tea + 로그 파싱/리포트 생성기 (최우주)
├── docs/       # 개발계획서, 진행보고서, 라이선스 점검 기록 (정수민)
└── .github/    # 이슈/PR 템플릿, CI 워크플로우
```

## 빠른 시작

```bash
# (작성 예정 - 2주차 CLI 뼈대 완성 후 업데이트)
```

## 기여하기

[CONTRIBUTING.md](./CONTRIBUTING.md) 참고 — 커밋 컨벤션, 브랜치 전략, 라이선스
체크 절차가 정리되어 있습니다.

## 라이선스

[MIT](./LICENSE) — 단, `kernel/` 하위 xv6 원본 코드는 원본 라이선스 문구를 그대로
유지합니다. 상세는 [`docs/LICENSE_NOTES.md`](./docs/LICENSE_NOTES.md) 참고
(팀원3 담당, 작성 예정).

## 참가 정보

- 2026 오픈소스 개발자대회 · 학생부문 · 자유과제 · 세부과제: 미정
- 경북대학교 컴퓨터학부

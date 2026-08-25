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

## Quick Start

### 1. 커널 빌드 + 부팅

```bash
cd kernel
make CPUS=2 CHAOS=1 SEED=1 qemu
```

### 2. QEMU 안에서 cow_test 실행

부팅되면 뜨는 xv6 셸에서:

```
$ cow_test
...
== ALL COW CHECKS PASSED ==
[RACEGRADER_DONE] 1|3
```

### 3. racegrader CLI로 반복 실행

```bash
cd cli
go run . run --kernel ../kernel --command cow_test --repeat 20 --cpus 2 --seed 1
```

`--repeat`만큼 매번 다른 SEED로 재부팅하며 실행하고, 결과를 마크다운 리포트로 남깁니다.

### 4. 결함 커널로 실제 버그 잡기

`kernel/`은 정상 커널(main)과, 검증용으로 결함을 주입한 브랜치(`test/kernel-*`)로 나뉘어 있습니다.
같은 조건(`race_stress`, CPUS=8, 500회)으로 두 쪽을 비교하면:

```bash
# (저장소 루트에서)
cd cli
go run . run --kernel ../kernel --command race_stress --repeat 500 --cpus 8 --timeout 60
#   정상 커널(main): Pass 500 / Fail 0

# 결함 커널: kalloc.c만 결함 버전으로 교체
# (브랜치 전체 checkout은 test/kernel-refcount-race가 8/9 이후 갱신 안 돼서
#  race_stress.c 등 최신 파일이 없어 실패할 수 있음 — 파일 단위 교체 권장)
cd ..
git show test/kernel-refcount-race:kernel/kernel/kalloc.c > kernel/kernel/kalloc.c
cd cli
go run . run --kernel ../kernel --command race_stress --repeat 500 --cpus 8 --timeout 60
#   결함 커널: Pass 499 / Fail 1
#   [RACEGRADER_FAIL] PANIC|kernel/trap.c:154|1479322835|...

# 테스트 끝나면 반드시 원상복구
cd ..
git checkout -- kernel/kernel/kalloc.c
```

기본 파라미터(`CHAOS_PROB_PERCENT=30`, `CHAOS_MAX_SPIN=2000`)로 500회 중 1회 재현 — 희귀하지만
실재하는 레이스이고, 반복 실행 없이는(한두 번만 돌려서는) 절대 발견할 수 없다는 것을 보여줍니다.
정상 커널은 같은 500회에서 0건, 즉 오탐 없이 통과합니다.

**향후 개선 방향**: 검출률을 더 높이려면 `chaos.c`의 `CHAOS_PROB_PERCENT`/`CHAOS_MAX_SPIN`을
올리는 방법도 있지만, 이는 커널 전체 락 타이밍에 영향을 줘 다른 테스트(오탐률 등)에도 부작용이
번집니다. 반면 `race_stress.c`의 `NRACERS`/`ROUNDS`를 늘리는 쪽은 이 테스트 안에서만 경쟁자
수·시행 횟수를 키우는 것이라, 커널 전체 타이밍에 영향을 주지 않고 검출률을 높일 수 있는 더
안전한 손잡이입니다.

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

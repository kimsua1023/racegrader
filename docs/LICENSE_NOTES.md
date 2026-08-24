# 라이선스 점검 기록 (팀원3 담당)

이 프로젝트가 가져다 쓰는 모든 외부 코드/라이브러리의 라이선스를 여기에 기록합니다.
**Permissive**(MIT, BSD, Apache-2.0) vs **Viral**(GPL, AGPL) 구분에 특히 주의하세요.
커널 코드는 특히 GPL 코드가 섞여 있지 않은지 신경 써야 합니다.

## xv6

| 항목 | 내용 |
|---|---|
| 원본 라이선스 | MIT License (SPDX: MIT) |
| 저작권자 | Frans Kaashoek, Robert Morris, Russ Cox, Massachusetts Institute of Technology (2006-2024) |
| 출처 | https://github.com/mit-pdos/xv6-riscv |
| 처리 방법 | 원본 LICENSE 파일(`kernel/LICENSE`) 및 소스 상단 주석 그대로 유지 |
| 확인일 | 2026-08-24 |
| 확인자 | 정수민 |

## Go 의존성 (`cli/`)

| 패키지 | 버전 | 라이선스(SPDX) | Permissive/Viral | 확인일 | 확인자 |
|---|---|---|---|---|---|
| charm.land/bubbletea/v2 | v2.0.8 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/spf13/cobra | v1.10.2 | Apache-2.0 | Permissive | 2026-08-24 | 정수민 |
| github.com/charmbracelet/x/ansi | v0.11.7 | MIT | Permissive | 2026-08-05 | 정수민 |
| golang.org/x/sys/unix | (간접 의존성) | BSD-3-Clause | Permissive | 2026-08-05 | 정수민 |
| github.com/aymanbagabas/go-udiff | v0.2.0 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/bits-and-blooms/bitset | v1.24.4 | BSD-3-Clause | Permissive | 2026-08-24 | 정수민 |
| github.com/charmbracelet/colorprofile | v0.4.3 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/charmbracelet/ultraviolet | v0.0.0-20260703 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/charmbracelet/x/exp/golden | v0.0.0-20241212 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/charmbracelet/x/term | v0.2.2 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/charmbracelet/x/termios | v0.1.1 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/charmbracelet/x/windows | v0.2.2 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/cpuguy83/go-md2man/v2 | v2.0.6 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/lucasb-eyer/go-colorful | v1.4.0 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/mattn/go-runewidth | v0.0.23 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/muesli/cancelreader | v0.2.2 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/rivo/uniseg | v0.4.7 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/russross/blackfriday/v2 | v2.1.0 | BSD-2-Clause | Permissive | 2026-08-24 | 정수민 |
| github.com/spf13/pflag | v1.0.9 | BSD-3-Clause | Permissive | 2026-08-24 | 정수민 |
| golang.org/x/exp | v0.0.0-20220909 | BSD-3-Clause | Permissive | 2026-08-24 | 정수민 |
| golang.org/x/sync | v0.21.0 | BSD-3-Clause | Permissive | 2026-08-24 | 정수민 |
| github.com/clipperhouse/displaywidth | v0.11.0 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/clipperhouse/stringish | v0.1.1 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/clipperhouse/uax29/v2 | v2.7.0 | MIT | Permissive | 2026-08-24 | 정수민 |
| github.com/inconshreveable/mousetrap | v1.1.0 | Apache-2.0 | Permissive | 2026-08-24 | 정수민 |
| github.com/xo/terminfo | v0.0.0-20220910 | MIT | Permissive | 2026-08-24 | 정수민 |
| go.yaml.in/yaml/v3 | v3.0.4 | Apache-2.0 | Permissive | 2026-08-24 | 정수민 |
| gopkg.in/check.v1 | v0.0.0-20161208 | BSD-2-Clause | Permissive | 2026-08-24 | 정수민 |


> `go-licenses check ./...` 실행 결과(로그 또는 CI 아티팩트 링크)를 여기에 첨부하거나
> 링크로 남기세요. 절차는 `CONTRIBUTING.md`의 "새 의존성 추가 시" 항목 참고.

## 점검 이력

| 주차 | 스캔 범위 | 결과 | 비고 |
|---|---|---|---|
| 1주차 | 잠정 의존성 목록 | | 등록 예정 |
| 2주차 | cli/ 신규 의존성 (bubbletea, cobra, x/ansi, x/sys/unix) | 전부 permissive 확인 | PR #4 |
| 4주차 | 전체 27개 의존성 (go list -m all 기준) | 전부 permissive 확정 (MIT/BSD/Apache-2.0), GPL/AGPL 없음 | go-licenses 도구 실행 불가(google/go-licenses#128, Go 1.26.5 auto-toolchain 버그) → go list -m all + 각 저장소 라이선스 배지 수동 확인으로 대체 |
| 5주차 (최종) | 전체 | | GPL/AGPL 계열 없음 확인 필요 |

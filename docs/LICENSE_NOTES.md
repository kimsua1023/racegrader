# 라이선스 점검 기록 (팀원3 담당)

이 프로젝트가 가져다 쓰는 모든 외부 코드/라이브러리의 라이선스를 여기에 기록합니다.
**Permissive**(MIT, BSD, Apache-2.0) vs **Viral**(GPL, AGPL) 구분에 특히 주의하세요.
커널 코드는 특히 GPL 코드가 섞여 있지 않은지 신경 써야 합니다.

## xv6

| 항목 | 내용 |
|---|---|
| 원본 라이선스 | (SPDX 식별자 확인 후 기입) |
| 처리 방법 | 원본 LICENSE 파일 및 소스 상단 주석 그대로 유지 |
| 확인일 | (작성 예정) |
| 확인자 | 정수민 |

## Go 의존성 (`cli/`)

| 패키지 | 버전 | 라이선스(SPDX) | Permissive/Viral | 확인일 | 확인자 |
|---|---|---|---|---|---|
| github.com/charmbracelet/bubbletea | v2.0.8 | MIT | Permissive | 2026-08-05 | 정수민 |
| github.com/spf13/cobra | v2.0.8 | Apache-2.0 | Permissive | 2026-08-05 | 정수민 |
| github.com/charmbracelet/x/ansi | v0.11.7 | MIT | Permissive | 2026-08-05 | 정수민 |
| golang.org/x/sys/unix | (간접 의존성) | BSD-3-Clause | Permissive | 2026-08-05 | 정수민 |

> `go-licenses check ./...` 실행 결과(로그 또는 CI 아티팩트 링크)를 여기에 첨부하거나
> 링크로 남기세요. 절차는 `CONTRIBUTING.md`의 "새 의존성 추가 시" 항목 참고.

## 점검 이력

| 주차 | 스캔 범위 | 결과 | 비고 |
|---|---|---|---|
| 1주차 | 잠정 의존성 목록 | | 등록 예정 |
| 2주차 | cli/ 신규 의존성 (bubbletea, cobra, x/ansi, x/sys/unix) | 전부 permissive 확인 | PR #4 |
| 5주차 (최종) | 전체 | | GPL/AGPL 계열 없음 확인 필요 |

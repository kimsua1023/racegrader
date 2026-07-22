# 기여 가이드 (CONTRIBUTING)

## 커밋 메시지 규칙 — Conventional Commits

형식: `<type>(<scope>): <설명>`

**type**

| type | 용도 |
|---|---|
| feat | 새 기능 추가 |
| fix | 버그 수정 |
| docs | 문서 변경 (README, 보고서 등) |
| refactor | 동작 변화 없는 코드 구조 개선 |
| test | 테스트 코드 추가/수정 |
| chore | 빌드 설정, 의존성 등 잡무 |

**scope** (이 프로젝트 전용)

- `kernel` — xv6 커널 코드 (팀원1 · 김수아)
- `cli` — Go CLI / Bubble Tea (팀원2 · 최우주)
- `docs` — 문서 / 보고서 / 라이선스 (팀원3 · 정수민)
- `ci` — GitHub Actions

예시:

```
feat(kernel): CoW fork에 refcount race 시나리오 추가
fix(cli): QEMU 타임아웃 시 좀비 프로세스 정리
docs(license): go-licenses 스캔 결과 반영
```

## 브랜치 전략

3인 소규모 팀 + 6주 스프린트이므로 **트렁크 기반(trunk-based) 경량 전략**을 씁니다.
별도 develop 브랜치 없이 `main` 하나만 보호(protected)하고, 모든 작업은 짧은 수명의
feature 브랜치에서 진행한 뒤 PR로 병합합니다.

브랜치 이름 규칙: `<type>/<scope>-<짧은설명>`

```
feat/kernel-fault-injection
feat/cli-bubbletea-ui
docs/license-scan-week1
fix/kernel-refcount-panic
```

**규칙**

1. `main`은 항상 데모 가능한 상태를 유지합니다 (깨진 코드 merge 금지).
2. 모든 변경은 PR을 통해서만 `main`에 들어갑니다. (직접 push 금지)
3. PR은 **다른 팀원 1명 이상의 승인**이 필요합니다.
   - 커널 ↔ CLI PR: 서로 리뷰 (김수아 ↔ 최우주)
   - 문서/라이선스 관련 PR: 전원 확인 권장 (특히 라이선스 헤더/SPDX 표기)
4. 로그 포맷처럼 **팀원 간 인터페이스가 걸린 변경**은 PR 제목/설명에
   `#interface-change`를 명시하고 관련 팀원을 리뷰어로 지정합니다.
   (팀원1의 실패 로그 형식 ↔ 팀원2의 로그 파서가 대표적인 예시입니다.)

## 새 의존성(라이브러리) 추가 시

CLI에 새 Go 패키지를 추가하면 **머지 전에** 아래를 실행하고 결과를 PR에 첨부하세요.

```bash
cd cli
go install github.com/google/go-licenses@latest
go-licenses check ./... 2>&1 | tee ../docs/license-check-$(date +%Y%m%d).log
```

GPL/AGPL 계열 라이선스가 잡히면 **팀원3(정수민)에게 즉시 공유** 후 대체 라이브러리를 검토합니다.
커널(`kernel/`) 쪽에 코드를 가져다 붙일 때도 동일하게, GPL 코드가 섞이는지 반드시 확인합니다.

## Issue / PR 작성

- Issue는 반드시 템플릿 중 하나를 사용합니다 (`.github/ISSUE_TEMPLATE/`).
- PR은 관련 Issue를 `Closes #번호` 형식으로 연결합니다.

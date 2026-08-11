# 커밋 컨벤션 (Conventional Commits)

형식:
```
<type>(<scope>): <subject>

<body (선택)>

<footer (선택, 이슈 연결 등)>
```

## Type
| type | 설명 |
|---|---|
| feat | 새로운 기능 추가 |
| fix | 버그 수정 |
| docs | 문서만 변경 (README, 개발계획서, 보고서 등) |
| refactor | 기능 변화 없는 코드 구조 변경 |
| test | 테스트 코드 추가/수정 |
| chore | 빌드, 설정, 의존성 등 기타 변경 |
| ci | GitHub Actions 등 CI 설정 변경 |

## Scope (권장, 생략 가능)
이슈 라벨 + PR 템플릿 scope와 동일하게 통일:
- `kernel` — xv6 커널, fault injection, scheduler
- `cli` — Go CLI, Bubble Tea UI, 로그 파싱
- `docs` — 문서, 개발계획서, 보고서, 라이선스
- `ci` — GitHub Actions, 빌드/워크플로 설정

> kernel의 실패 로그 형식을 바꾸는 커밋은 반드시 footer에 `interface-change`를
> 명시하고, PR에도 같은 라벨을 달아 팀원2에게 알릴 것.

## Subject 규칙
- 명령형, 소문자 시작, 끝에 마침표 없음
- 50자 이내 권장
- 예: `feat(kernel): add scheduler perturbation for race injection`

## 예시
```
docs(docs): draft 개발계획서 초안

feat(cli): implement bubble tea progress view

fix(kernel): fix deadlock in fault injector cleanup

Closes #12
```

## Body / Footer
- Body: 무엇을/왜 변경했는지 (어떻게는 코드가 말해줌)
- Footer: `Closes #이슈번호`, `BREAKING CHANGE: ...` 등

## 커밋 메시지 템플릿 적용 (선택)
로컬에 커밋 템플릿을 등록해두면 `git commit` 시 가이드가 자동으로 뜹니다.

```bash
git config commit.template .gitmessage
```

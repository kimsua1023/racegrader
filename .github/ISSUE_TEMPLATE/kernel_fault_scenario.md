---
name: "🐛 커널 결함 시나리오"
about: xv6 결함 재현 / 스케줄링 교란 / 실패 탐지 관련 작업 (팀원1 담당)
title: "[kernel] "
labels: kernel
assignees: ""
---

## 시나리오 설명
<!-- 예: CoW fork에서 refcount race를 재현하는 버전 -->

## 재현 방법
```bash
# 실행 커맨드
```

## 예상 실패 로그 형식
<!-- 팀원2(CLI)의 로그 파서가 이 형식을 그대로 읽습니다.
     형식을 바꾸면 반드시 #interface-change 라벨과 함께 팀원2를 태그하세요. -->
```
[FAIL] proc.c:55 refcount<0
```

## 완료 조건 (Definition of Done)
- [ ] 정상 버전 / 결함 버전 각각 QEMU에서 실행 확인
- [ ] 로그 형식이 팀원2와 합의된 포맷과 일치

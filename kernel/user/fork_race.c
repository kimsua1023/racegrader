// user/fork_race.c — RaceGrader concurrency stress test for the
// krefinc() lock-missing fault (test/kernel-lock-missing 시나리오 전용)
//
// race_stress.c는 kfree() 경로(cowhandler write-fault)의 경쟁만 잡는다.
// krefinc()는 한 부모의 순차 fork() 루프에서만 불려서 그 안에서는
// 경쟁이 생길 수 없다(순차 호출은 자동으로 안전) — race_stress.c로는
// 이 버그를 절대 잡을 수 없다.
//
// 이 테스트는 조부모(GP) -> 자식(C1..Cn) -> 손자(G1..Gn) 3세대 구조를
// 쓴다: GP가 C1..Cn을 순차 fork(안전)한 뒤, barrier로 C1..Cn을 "동시에"
// fork시켜 각자 손자를 만들게 한다 — 이 순간 C1..Cn이 서로 다른 CPU에서
// 동시에 krefinc(p)를 호출하게 되어 진짜 경쟁이 열린다.
//
// 검증 트릭: GP는 절대 자기 몫을 놓지 않는다(exit도 write도 안 함).
// 진짜 refcount = 1(GP) + n(C1..Cn) + n(G1..Gn). 손자/자식이 전부
// release(2n번)하고 나면 정상이라면 추적값은 1(GP 몫)에서 멈춰야
// 한다. krefinc() 증가분이 유실됐다면 release 도중 추적값이 0을
// 찍어 페이지가 조기 반납되는데, GP의 PTE는 여전히 그 물리주소를
// 가리키고 있으므로 GP가 "아직 살아있는 마지막 증인"이 된다.
// GP가 같이 release하면 이 모순(추적값=0 vs 진짜 소유자 생존) 자체가
// 사라져서 버그와 정상 케이스를 구분할 수 없다 — 그래서 GP는 끝까지
// 물러나면 안 된다.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/riscv.h"

#ifndef CHAOS_SEED
#define CHAOS_SEED 1
#endif

#define NKIDS  16
#define ROUNDS 40

static void
die(const char *msg)
{
  printf("%s\n", msg);
  exit(1);
}

static void
assert_impl(const char *label, int pass, const char *file, int line)
{
  printf("%s: %s\n", label, pass ? "OK" : "FAIL");
  if(!pass){
    printf("[RACEGRADER_FAIL] ASSERT|%s:%d|%d|%d|%s\n", file, line, CHAOS_SEED, getpid(), label);
    exit(1);
  }
}
#define check(label, pass) assert_impl((label), (pass), __FILE__, __LINE__)

static void
do_round(int round)
{
  char *p = sbrk(PGSIZE);
  if(p == (char*)-1)
    die("sbrk failed");
  p[0] = 'A';
  uint64 target_pa = ptepa(p); // GP 단독 소유, 진짜 refcount = 1

  int readyfd[2]; // 자식 -> GP: "준비 완료"
  int gofd[2];    // GP -> 자식: "출발" (barrier)
  if(pipe(readyfd) < 0 || pipe(gofd) < 0)
    die("pipe failed");

  int nkids = 0;
  for(int i = 0; i < NKIDS; i++){
    int pid = fork();
    if(pid < 0)
      break;

    if(pid == 0){
      // Ci: 순차 fork로 생성됨 — 여기까진 경쟁 없음
      close(readyfd[0]);
      close(gofd[1]);
      char c = 'r';
      write(readyfd[1], &c, 1); // 준비 완료 알림
      read(gofd[0], &c, 1);     // 출발 신호 대기

      // *** 경쟁 지점 ***
      // 여러 Ci가 서로 다른 CPU에서 "동시에" fork() -> krefinc(p) 동시 호출
      int gpid = fork();
      if(gpid < 0)
        exit(1);

      if(gpid == 0)
        exit(0); // Gi: 자기 몫만 반납하고 즉시 종료

      wait(0);   // Gi를 반드시 reap한 뒤에야 Ci 자신도 종료
                 // (release 순서 보장 — race_stress v1이 겪은 실수 방지)
      exit(0);
    }
    nkids++;
  }
  close(readyfd[1]);
  close(gofd[0]);

  for(int i = 0; i < nkids; i++){
    char c;
    read(readyfd[0], &c, 1);
  }

  for(int i = 0; i < nkids; i++){
    char c = 'g';
    write(gofd[1], &c, 1); // 전원에게 동시에 출발 신호 브로드캐스트
  }

  for(int i = 0; i < nkids; i++)
    wait(0); // Ci 전원 reap — 이 시점에 2*nkids번의 release가 "확실히" 끝남

  close(readyfd[0]);
  close(gofd[1]);

  // GP는 여기서 절대 자기 몫을 놓지 않는다 (파일 상단 주석 참고)

  check("[G1] page content intact after full release cycle", p[0] == 'A');

  // R5 스타일: freelist 재사용 여부를 물리주소로 확정 확인.
  // content 체크는 freed page가 아직 재사용 안 됐으면 놓칠 수 있어서
  // (freelist에만 들어가고 내용은 그대로일 수 있음) 이 체크로 메운다.
  char *w1 = sbrk(PGSIZE);
  char *w2 = sbrk(PGSIZE);
  if(w1 != (char*)-1)
    check("[G2] fresh alloc does not reuse still-owned physical page",
          ptepa(w1) != target_pa);
  if(w2 != (char*)-1)
    check("[G3] fresh alloc (2nd probe) does not reuse still-owned physical page",
          ptepa(w2) != target_pa);
}

int
main(void)
{
  for(int round = 0; round < ROUNDS; round++){
    do_round(round);
  }

  char *q = sbrk(PGSIZE);
  check("[G4] allocator still works after stress (sbrk succeeds)", q != (char*)-1);
  if(q != (char*)-1){
    q[0] = 'X';
    check("[G5] freshly allocated page is writable and holds written value", q[0] == 'X');
  }

  printf("fork_race: %d rounds x concurrent-fork krefinc racers completed\n", ROUNDS);
  printf("[RACEGRADER_DONE] %d|%d\n", CHAOS_SEED, getpid());
  exit(0);
}

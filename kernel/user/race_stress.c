// user/race_stress.c — RaceGrader concurrency stress test (v4: 반복 루프)
//
// v3까지의 문제: QEMU 재부팅 1회당 경쟁 시도가 딱 1번뿐이라, 부팅 오버헤드
// 대비 "진짜 승부의 순간"이 너무 적었다. genuine SMP는 smp_check.c로 이미
// 확인됨(직렬 아님) — 그럼 남은 건 시도 횟수 자체를 늘리는 것뿐이다.
// v4는 같은 부팅 안에서 pipe 바리어 경쟁 라운드를 ROUNDS번 반복한다.
//
// v2의 문제(추정): 부모가 fork 루프를 다 돈 뒤에야 자기 몫을 release했는데,
// 그 사이 자식들(write 없이 바로 exit)이 이미 대부분 먼저 끝나서 서로 안
// 겹치고 순서대로 release가 끝났을 가능성이 높다. "각자 알아서 빨리 끝내기"
// 방식으로는 진짜 "동시"를 보장할 수 없다.
//
// v3: pipe 2개로 바리어(barrier)를 만든다. 모든 참가자(자식들 + 부모)를
// 먼저 "준비 완료" 상태로 만들어놓고, 부모가 한꺼번에 "출발" 신호를 보내는
// 바로 그 순간 다 같이 release를 시도하게 강제한다 — 우연히 타이밍이
//맞기를 기다리지 않고, 동시성을 직접 설계해서 만든다.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/riscv.h"

#ifndef CHAOS_SEED
#define CHAOS_SEED 1
#endif

#define NRACERS 8

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

#define ROUNDS 40

static int
do_round(int round)
{
  char *p = sbrk(PGSIZE);
  if(p == (char*)-1)
    die("sbrk failed");
  p[0] = 'A'; // 부모 단독 소유, refcount=1

  int readyfd[2]; // 자식 -> 부모: "준비 완료"
  int gofd[2];    // 부모 -> 자식: "출발" (barrier)
  if(pipe(readyfd) < 0 || pipe(gofd) < 0)
    die("pipe failed");

  int nkids = 0;
  for(int i = 0; i < NRACERS; i++){
    int pid = fork();
    if(pid < 0)
      break;

    if(pid == 0){
      close(readyfd[0]);
      close(gofd[1]);
      char c = 'r';
      write(readyfd[1], &c, 1); // 준비 완료 알림
      read(gofd[0], &c, 1);     // 출발 신호 대기 (여기서 다 같이 멈춰있음)
      exit(0);                  // 신호 받는 즉시 전원 동시 종료 -> kfree() 경쟁
    }
    nkids++;
  }
  close(readyfd[1]);
  close(gofd[0]);

  // 자식들이 전부 "준비 완료" 상태(출발선에 서서 대기 중)가 될 때까지 기다린다.
  for(int i = 0; i < nkids; i++){
    char c;
    read(readyfd[0], &c, 1);
  }

  // 부모도 자기 몫을 놓아버린다 -- 자식들에게 출발 신호를 주기 직전에 먼저
  // 실행해서, 부모의 release도 이 "동시 출발" 그룹에 최대한 가깝게 포함시킨다.
  if(sbrk(-PGSIZE) == (char*)-1)
    die("sbrk shrink failed");

  // 전 자식에게 동시에 출발 신호 -> 전부 한꺼번에 깨어나 exit() (kfree 경쟁)
  for(int i = 0; i < nkids; i++){
    char c = 'g';
    write(gofd[1], &c, 1);
  }

  for(int i = 0; i < nkids; i++)
    wait(0);

  // 부모가 이번 라운드에서 계속 쓰던 fd들을 명시적으로 닫는다.
  // 안 닫으면 라운드가 반복될 때마다 누적돼서, xv6의 프로세스당 열린
  // 파일 개수 제한(NOFILE)에 금방 부딪혀 이후 라운드의 pipe()가 실패한다.
  close(readyfd[0]);
  close(gofd[1]);

  return nkids;
}

int
main(void)
{
  for(int round = 0; round < ROUNDS; round++){
    do_round(round);
  }

  // 커널 할당자가 여전히 정상 동작하는지 사후검사 (ROUNDS번 다 돌고 나서
  // 최종적으로 한 번). R3/R4만으론 부족해서(self-loop 오염을 원리상 못
  // 잡음) R5까지 같이 확인한다.
  char *q = sbrk(PGSIZE);
  check("[R3] allocator still works after stress (sbrk succeeds)", q != (char*)-1);
  if(q != (char*)-1){
    q[0] = 'X';
    check("[R4] freshly allocated page is writable and holds written value", q[0] == 'X');
  }

#define NCHECK 8
  uint64 pas[NCHECK];
  int dup_found = 0;
  int allocated = 0;
  for(int i = 0; i < NCHECK; i++){
    char *r = sbrk(PGSIZE);
    if(r == (char*)-1)
      break;
    r[0] = 'Y';
    uint64 pa = ptepa((void*)r);
    for(int j = 0; j < allocated; j++){
      if(pas[j] == pa)
        dup_found = 1;
    }
    pas[allocated++] = pa;
  }
  check("[R5] no duplicate physical pages among fresh allocations (freelist not corrupted into a self-loop)", !dup_found);

  printf("race_stress: %d rounds x pipe-barrier racers completed\n", ROUNDS);
  printf("[RACEGRADER_DONE] %d|%d\n", CHAOS_SEED, getpid());
  exit(0);
}

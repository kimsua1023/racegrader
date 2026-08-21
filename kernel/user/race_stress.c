// user/race_stress.c — RaceGrader concurrency stress test (v5)
//
// v2/v3/v4의 근본 문제 발견: kwait()이 전역 락(wait_lock, 프로세스별이
// 아니라 시스템 전체에 하나)을 쥔 채로 freeproc()->kfree()를 호출한다.
// 즉 exit()/wait() 경로로 유발되는 kfree()는 이 전역 락 때문에 애초에
// 동시에 두 개가 진행될 수 없다 -- 바리어를 아무리 정교하게 만들어도
// 소용없었던 이유가 이것이다.
//
// v5: kfree()로 가는 또 다른 경로인 cowhandler()(write fault로 유발되는
// COW 분리)를 쓴다. 이건 전역 락을 거치지 않고 각 프로세스가 독립적으로
// write fault를 처리하며 kfree()를 부른다 -- 진짜 경쟁이 가능한 경로다.
// v1이 이 경로를 썼었지만 그때는 부모가 끝까지 자기 몫을 안 놓아서
// refcount가 0에 안 닿는 문제가 있었다. v5는 부모도 exit이 아니라
// write로, 자식들과 함께 파이프 바리어를 통해 동시에 참여시킨다.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/riscv.h"

#ifndef CHAOS_SEED
#define CHAOS_SEED 1
#endif

#define NRACERS 16
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
      read(gofd[0], &c, 1);     // 출발 신호 대기
      // exit()이 아니라 write로 COW 분리를 유발 -> cowhandler() -> kfree()
      // (전역 락 없이 독립적으로 실행되는, 진짜 경쟁 가능한 경로)
      p[0] = (char)('a' + (round % 26));
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

  // 자식들에게 출발 신호를 먼저 보내고, 부모도 곧바로 자기 몫을 write
  // -> 부모의 cowhandler()/kfree()도 자식들과 "동시에" 경쟁에 참여
  for(int i = 0; i < nkids; i++){
    char c = 'g';
    write(gofd[1], &c, 1);
  }
  p[0] = 'Z';

  for(int i = 0; i < nkids; i++)
    wait(0);

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

  printf("race_stress: %d rounds x write-triggered cowhandler racers completed\n", ROUNDS);
  printf("[RACEGRADER_DONE] %d|%d\n", CHAOS_SEED, getpid());
  exit(0);
}

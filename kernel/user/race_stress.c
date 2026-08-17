// user/race_stress.c — RaceGrader concurrency stress test (v2)
//
// v1의 문제점(교훈으로 남김): 자식이 먼저 write해서 COW 분리(자기 전용 새
// 페이지로 교체)해버리면, 공유 페이지의 kfree()는 "많은 참조자 중 하나가
// 빠지는" 정도로만 refcount를 줄일 뿐, 실제로 0까지 내려가는 순간을 절대
// 안 만든다 — 부모가 마지막까지 refcount==1인 채로 fast-path만 타고
// kfree()를 한 번도 안 불렀기 때문이다. refcount-race 버그는 정확히
// "카운트가 0으로 떨어지는 그 순간"에 두 프로세스가 동시에 도착해야
// 재현되는데, v1은 그 순간 자체를 만든 적이 없다 (13->1까지만 내려감).
//
// v2: 자식들은 write를 아예 안 하고 곧바로 exit() 한다. 프로세스 종료 시
// 커널이 그 프로세스가 매핑한 페이지에 자동으로 kfree()를 부르는데, 이번엔
// write를 안 했으니 COW 분리 없이 "진짜 공유 페이지 그 자체"에 kfree()가
// 걸린다. 부모도 자식들을 기다리지 않고 곧바로 자기 몫을 sbrk(-PGSIZE)로
// 놓아버려서, 부모의 release가 자식들의 release와 진짜로 동시에 경쟁하게
// 만든다 — refcount가 0을 향해 내려가는 마지막 순간에 여러 release가
// 실제로 겹칠 기회를 만드는 것이 핵심이다.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/riscv.h"

#ifndef CHAOS_SEED
#define CHAOS_SEED 1
#endif

#define NCHILD 12

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

int
main(void)
{
  char *p = sbrk(PGSIZE);
  if(p == (char*)-1)
    die("sbrk failed");

  p[0] = 'A'; // 페이지를 실제로 매핑시킴 (부모 단독 소유, refcount=1)

  int nkids = 0;

  // 자식들을 최대한 몰아서 fork. 각 자식은 write 없이 곧바로 종료 ->
  // exit()의 페이지 정리 과정이 공유 페이지에 직접 kfree()를 부르게 한다.
  for(int i = 0; i < NCHILD; i++){
    int pid = fork();
    if(pid < 0)
      break; // 자원이 모자라면 지금까지 만든 것만으로 계속 진행

    if(pid == 0){
      exit(0);
    }
    nkids++;
  }

  // 부모도 자식들을 기다리지 않고 곧바로 자기 몫을 놓아버린다.
  // -> 부모의 release가 자식들이 한창 exit()하며 release하는 것과
  //    시간적으로 겹치게 만드는 것이 핵심이다.
  if(sbrk(-PGSIZE) == (char*)-1)
    die("sbrk shrink failed");
  // 주의: 이 시점부터 p를 더 이상 읽거나 쓰면 안 된다 (우리 쪽 use-after-free).

  for(int i = 0; i < nkids; i++)
    wait(0);

  // 여기까지 살아남았다는 것 자체가 이미 유의미한 신호(패닉/더블프리로
  // 안 죽었다는 뜻). 추가로, 커널 할당자가 여전히 정상 동작하는지도
  // 확인한다 — double-free로 freelist가 오염되면 이후 kalloc()이
  // 이상하게 동작할 수 있기 때문에, 이건 이 특정 버그에 잘 맞는 사후검사다.
  char *q = sbrk(PGSIZE);
  check("[R3] allocator still works after stress (sbrk succeeds)", q != (char*)-1);
  if(q != (char*)-1){
    q[0] = 'X';
    check("[R4] freshly allocated page is writable and holds written value", q[0] == 'X');
  }

  printf("race_stress: %d children forked, all released shared page concurrently\n", nkids);
  printf("[RACEGRADER_DONE] %d|%d\n", CHAOS_SEED, getpid());
  exit(0);
}

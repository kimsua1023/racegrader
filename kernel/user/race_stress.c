// user/race_stress.c — RaceGrader concurrency stress test
//
// 목적: 하나의 공유(COW) 페이지에 대해 여러 자식이 "거의 동시에" krefinc()/kfree()를
// 건드리도록 몰아서, test/kernel-lock-missing / test/kernel-refcount-race 결함
// 시나리오가 실제로 노출되는지 확인한다.
//
// cow_test.c는 fork()를 딱 한 번만 하는 "정답 검증용" 테스트라, refcount에 대한
// 경쟁 상황 자체가 생기지 않는다 (경쟁할 상대가 없음). 이 프로그램은 그 반대로,
// 정답 검증이 아니라 "경쟁 상황을 실제로 만드는 것"에만 집중한다.

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

// cow_test.c와 동일한 [RACEGRADER_FAIL] ASSERT 형식으로 실패를 알린다.
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

  p[0] = 'A'; // 부모 소유의 페이지로 확실히 매핑시킴 (W=1, no COW)

  int nkids = 0;

  // 자식들을 하나씩 기다리지 않고 최대한 몰아서 fork.
  // -> uvmcopy()의 krefinc() 호출들이 시간적으로 겹칠 기회를 최대화한다.
  for(int i = 0; i < NCHILD; i++){
    int pid = fork();
    if(pid < 0)
      break; // 자원이 모자라면 지금까지 만든 것만으로 계속 진행

    if(pid == 0){
      // 자식: 곧바로 자기 몫에 쓰기 -> COW 분리(cowhandler -> kfree) 유발.
      // 여러 자식이 "거의 동시에" 이걸 하면서 kfree()의 감소/0체크 구간에도
      // 경쟁을 만든다.
      p[0] = (char)('a' + (i % 26));
      exit(0);
    }
    nkids++;
  }

  for(int i = 0; i < nkids; i++)
    wait(0);

  // 무결성 확인: 자식들은 전부 "자기 자신의 COW 분리된 복사본"에만 썼어야 하므로,
  // 부모의 원본 페이지는 여전히 'A' 그대로여야 한다. lock-missing/refcount-race
  // 버그로 인해 이 물리 페이지가 조기 반납되고 다른 용도로 재사용됐다면,
  // 여기서 값이 달라져 있을 것이다.
  check("[R1] parent page untouched by children after stress fork", p[0] == 'A');

  p[0] = 'Z'; // 부모도 마지막에 한 번 더 써서 fast-path(kgetrefc==1)도 건드려본다.
  check("[R2] parent write after stress succeeds", p[0] == 'Z');

  printf("race_stress: %d children forked/joined\n", nkids);
  printf("[RACEGRADER_DONE] %d|%d\n", CHAOS_SEED, getpid());
  exit(0);
}

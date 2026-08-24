// user/smp_check.c — 진짜 2-way 하드웨어 병렬성이 있는지 확인하는 최소 실험
//
// race_stress.c의 세 번의 재설계(v1/v2/v3)가 서로 완전히 다른 메커니즘을
// 노렸는데도 똑같이 0/200으로 실패했다. 이건 "시나리오 설계 문제"보다
// "애초에 CPUS=2가 진짜 2-way 병렬을 만들어주는가"라는 더 근본적인 전제를
// 의심해봐야 한다는 신호일 수 있다. 이 프로그램은 그것만 확인한다.
//
// 방법: 부모 혼자 CPU를 일정량 소모하는 시간 vs 부모+자식이 동시에
// 각자 CPU를 소모하는 시간을 비교한다.
// - 진짜 병렬이면: 둘 다 비슷해야 한다
// - 사실상 직렬이면: 후자가 거의 2배 걸려야 한다

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#ifndef CHAOS_SEED
#define CHAOS_SEED 1
#endif

#define WORK 300000000UL

static void
burn(void)
{
  volatile unsigned long i;
  for(i = 0; i < WORK; i++)
    ;
}

int
main(void)
{
  uint64 t0, t1, solo_ticks, pair_ticks;

  t0 = uptime();
  burn();
  t1 = uptime();
  solo_ticks = t1 - t0;

  t0 = uptime();
  int pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }
  if(pid == 0){
    burn();
    exit(0);
  }
  burn();
  wait(0);
  t1 = uptime();
  pair_ticks = t1 - t0;

  printf("smp_check: solo=%d ticks, pair(parent+child)=%d ticks\n",
         (int)solo_ticks, (int)pair_ticks);

  // 진짜 병렬이면 pair가 solo보다 크게 안 늘어나야 한다.
  // 완전 직렬이면 거의 2배. 1.5배를 기준선으로 삼는다.
  int ratio_ok = (pair_ticks * 2 <= solo_ticks * 3);
  if(!ratio_ok){
    printf("[RACEGRADER_FAIL] ASSERT|smp_check.c:0|%d|%d|pair time not much less than 2x solo (solo=%d pair=%d) -- SMP may not be truly parallel\n",
           CHAOS_SEED, getpid(), (int)solo_ticks, (int)pair_ticks);
    exit(1);
  }

  printf("[RACEGRADER_DONE] %d|%d\n", CHAOS_SEED, getpid());
  exit(0);
}

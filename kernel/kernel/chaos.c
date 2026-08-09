// RaceGrader chaos engine — 스케줄링 교란 엔진
//
// 목적: 학생/팀 제출 커널 코드 내부 구조를 몰라도, 락 경계 근처의 타이밍을
// 흔들고(옵션2) 스케줄러의 프로세스 선택 순서를 매 라운드 셔플(옵션3)해서,
// 반복 실행(N회) 시 다양한 인터리빙 패턴이 나오도록 만든다.
//
// 안전 설계 원칙 (중요):
//  1. 여기서 절대 yield()/sched()를 직접 호출하지 않는다. xv6는 "swtch() 도중엔
//     정확히 p->lock 하나만 held 상태"라는 불변식을 요구하는데, 임의의 락(예:
//     refcnt.lock)을 쥔 채로 yield()를 부르면 이 불변식이 깨져 데드락/패닉으로
//     이어질 수 있다. 대신 busy-wait(스핀)로 "시간"만 늘려서, 원래 있던 타이머
//     인터럽트가 자연스럽게 그 구간에 걸릴 확률만 높인다.
//  2. 인터럽트가 꺼져있는 구간(= 락을 쥐고 있는 중, 또는 중첩 락 안)에서는
//     스핀해봐야 어차피 타이머 인터럽트가 못 들어와 아무 효과가 없다. 그래서
//     반드시 intr_get()으로 "인터럽트 켜진 상태"를 먼저 확인하고, 그때만
//     지연을 넣는다 — 이 덕분에 scheduler() 자신의 p->lock 같은 내부 락은
//     자동으로 건드리지 않게 된다 (그 구간은 항상 인터럽트 꺼진 채로 도니까).
//  3. PRNG 상태는 CPU별로 독립된 배열에 저장하고 락을 걸지 않는다. 정확한
//     원자성이 필요 없는 용도(그냥 무작위성)이기 때문이고, 오히려 여기에
//     락을 걸면 "락 진입 직전 훅"이 그 락 자신을 다시 부르는 재귀/데드락
//     위험이 생긴다. 상태 레이스는 의도적으로 허용한다(무해함).

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

#ifndef CHAOS_SEED
#define CHAOS_SEED 1
#endif

#ifndef CHAOS_ENABLED
#define CHAOS_ENABLED 1
#endif

#ifndef CHAOS_PROB_PERCENT
#define CHAOS_PROB_PERCENT 30   // 지연을 발동시킬 확률 (%)
#endif

#ifndef CHAOS_MAX_SPIN
#define CHAOS_MAX_SPIN 2000     // 최대 스핀 횟수 (명령어 수준, 아주 짧음)
#endif

// CPU별 독립 PRNG 상태. 0으로 초기화된 상태(=chaos_init 호출 전)에서도
// xorshift가 안전하게 항상 0을 반환하도록 되어 있어(퇴화 상태) 부팅 초반
// chaos_init() 호출 전에 있는 acquire()/release() 호출들에서도 크래시 없이
// 그냥 무동작(no-op)으로 지나간다.
static uint32 chaos_state[NCPU];

// CPU별 시드 초기화. main()에서 스케줄러 진입 직전, 모든 CPU가 각자 호출한다.
void
chaos_init(void)
{
  int id = cpuid();
  uint32 seed = ((uint32)CHAOS_SEED ^ 0x9E3779B9u) + (uint32)id * 0x85EBCA6Bu;
  if(seed == 0)
    seed = 1 + (uint32)id;
  chaos_state[id] = seed;
}

// xorshift32. CPU별 상태만 건드리므로 락이 필요 없다.
static uint32
chaos_rand(void)
{
  int id = cpuid();
  uint32 x = chaos_state[id];
  if(x == 0)
    return 0;   // chaos_init() 호출 전(퇴화 상태): 항상 0 -> 아래 확률 체크에서 자연히 걸리되 스핀 횟수도 0
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  chaos_state[id] = x;
  return x;
}

// 순수 busy-wait. 컨텍스트 스위치를 직접 유발하지 않는다.
static void
chaos_spin(uint32 iters)
{
  volatile uint32 i;
  for(i = 0; i < iters; i++)
    ;
}

// acquire() 진입 "직전"(아직 인터럽트가 켜져 있는 시점)에 호출.
void
chaos_delay_before_lock(void)
{
#if CHAOS_ENABLED
  if(!intr_get())
    return;   // 이미 다른 락 안(중첩)이면 스핀해봐야 효과 없음 -> 스킵
  if(chaos_rand() % 100 < CHAOS_PROB_PERCENT){
    chaos_spin(chaos_rand() % CHAOS_MAX_SPIN);
  }
#endif
}

// release() 완료 "직후"(인터럽트가 원래 상태로 복구된 뒤)에 호출.
void
chaos_delay_after_unlock(void)
{
#if CHAOS_ENABLED
  if(!intr_get())
    return;
  if(chaos_rand() % 100 < CHAOS_PROB_PERCENT){
    chaos_spin(chaos_rand() % CHAOS_MAX_SPIN);
  }
#endif
}

// Fisher-Yates 셔플. scheduler()가 매 라운드 proc 스캔 순서를 바꿀 때 사용.
// 인터럽트 꺼진 상태에서 호출돼도 안전하다 (busy-wait이 없는 순수 연산).
void
chaos_shuffle(int *order, int n)
{
#if CHAOS_ENABLED
  int i, j, tmp;
  for(i = n - 1; i > 0; i--){
    j = (int)(chaos_rand() % (uint32)(i + 1));
    tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }
#endif
}

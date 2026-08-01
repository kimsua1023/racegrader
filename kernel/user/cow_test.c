// user/cow_test.c — COW self-check
// - Child: after fork → same PA, W=0, (COW=1)
// - Child: after write → NEW PA, W=1, (COW=0)
// - Parent: after write → keep original PA, W=1, (COW=0)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/riscv.h"   // PTE_* macros, PGSIZE

// ---------- pretty printing ----------
static void die(const char *msg){ printf("%s\n", msg); exit(1); }
static void passfail(const char *label, int pass){
  printf("%s: %s\n", label, pass ? "OK" : "FAIL");
  if(!pass) exit(1);
}
static void line(void){ printf("------------------------------------------------------------\n"); }
static void section(const char *title){ line(); printf("%s\n", title); line(); }

// flags -> compact string like "VRW-XU[C]"
static void fmt_flags(uint64 f, char *out, int n){
  int k = 0;
  if(n < 10) return;
  out[k++] = (f & PTE_V) ? 'V' : '-';
  out[k++] = (f & PTE_R) ? 'R' : '-';
  out[k++] = (f & PTE_W) ? 'W' : '-';
  out[k++] = (f & PTE_X) ? 'X' : '-';
  out[k++] = (f & PTE_U) ? 'U' : '-';
  out[k++] = (f & PTE_COW) ? 'C' : '-';
  out[k] = 0;
}

static void show_mem(const char *tag) {
  int fp = freepages();
  printf("mem  | %s freepages=%d\n", tag, fp);
}

static uint64 page_va(void *va) { return PGROUNDDOWN((uint64)va); }

static uint64 get_flags(void *va){
  uint64 f = pteflags((void*)page_va(va));
  return (long)f == -1 ? 0 : f;
}
static uint64 get_pa(void *va){
  uint64 pa = ptepa((void*)page_va(va));
  return pa;
}

static void show_map(const char *tag, void *va){
  uint64 f  = pteflags((void*)page_va(va));
  uint64 pa = ptepa((void*)page_va(va));
  if((long)f == -1 || pa == (uint64)-1){
    printf("v2p  | %s va=%p (UNMAPPED)\n", tag, (void*)page_va(va));
    return;
  }
  char fl[12]; fmt_flags(f, fl, sizeof(fl));
  printf("v2p  | %s va=%p -> pa=%p flags=%s\n",
         tag, (void*)page_va(va), (void*)pa, fl);
}

int
main(void)
{
  section("COW self-check");

  // 0) Baseline
  printf("step 0: baseline → show free pages\n");
  show_mem("start");

  // 1) Parent alloc+touch one page
  printf("\nstep 1: parent alloc+touch 1 page → expect W=1, no COW\n");
  char *p = sbrk(PGSIZE);
  if(p == (char*)-1) die("sbrk failed");
  p[0] = 'A';
  show_mem("after sbrk+touch");
  show_map("parent: alloc", p);

  // Record parent baseline PA/flags before fork
  uint64 pa_parent0 = get_pa(p);
  uint64 fl_parent0 = get_flags(p);
  (void)fl_parent0;

  // 2) Fork
  printf("\nstep 2: fork → child shares same PA via COW; W=0 (and COW=1)\n");
  int pid = fork();
  if(pid < 0) die("fork failed");

  if(pid == 0){
    // ---- child ----
    section("child");

    // After fork: expect same PA as parent, W=0, +COW (if defined)
    uint64 pa_c_fork = get_pa(p);
    uint64 fl_c_fork = get_flags(p);
    show_mem("child: after fork");
    show_map("child: after fork", p);

    passfail("[C1] child shares PA after fork", pa_c_fork == pa_parent0);
    passfail("[C1] child W=0 after fork", (fl_c_fork & PTE_W) == 0);
    passfail("[C1] child COW=1 after fork", (fl_c_fork & PTE_COW) != 0);

    // First write in child → expect NEW PA, W=1, COW=0
    printf("\nchild writes one byte → expect NEW PA, W=1, COW=0\n");
    p[0] = 'C';

    uint64 pa_c_wr = get_pa(p);
    uint64 fl_c_wr = get_flags(p);
    show_mem("child: after write");
    show_map("child: after write", p);

    passfail("[C2] child split to NEW PA", pa_c_wr != pa_parent0);
    passfail("[C2] child W=1 after write", (fl_c_wr & PTE_W) != 0);
    passfail("[C2] child COW=0 after write", (fl_c_wr & PTE_COW) == 0);

    exit(0);
  } else {
    // Ensure child finishes first
    wait(0);

    // ---- parent ----
    section("parent");

    // Parent writes:
    // - keep the original PA (no allocation/copy)
    printf("parent writes one byte → keep PA, set W=1, clear COW\n");
    p[0] = 'P';

    uint64 pa_p_wr = get_pa(p);
    uint64 fl_p_wr = get_flags(p);
    show_mem("parent: after parent write");
    show_map("parent: after parent write", p);

    passfail("[P2] parent W=1 after write", (fl_p_wr & PTE_W) != 0);
    passfail("[P2] parent COW=0 after write", (fl_p_wr & PTE_COW) == 0);
    passfail("[P2] parent kept original PA", pa_p_wr == pa_parent0);

    line();
    printf("== ALL COW CHECKS PASSED ==\n");
    exit(0);
  }
}


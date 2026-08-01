#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/riscv.h"

#ifndef PROT_NONE
#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4
#endif

// ---------- Common printing helpers ----------
static void die(const char *msg){ printf("%s\n", msg); exit(1); }
static void passfail(const char *label, int pass){
  printf("%s: %s\n", label, pass ? "OK" : "FAIL");
  if(!pass) exit(1);
}

// Pretty-print PTE flags as a compact string like "VRW-XU"
static void fmt_flags(uint64 f, char *out){
  int k = 0;
  out[k++] = (f & PTE_V) ? 'V' : '-';
  out[k++] = (f & PTE_R) ? 'R' : '-';
  out[k++] = (f & PTE_W) ? 'W' : '-';
  out[k++] = (f & PTE_X) ? 'X' : '-';
  out[k++] = (f & PTE_U) ? 'U' : '-';
  out[k] = 0;
}

// Show PTE flags of a given user VA; "UNMAPPED" if there is no user mapping
static void show_flags(const char *tag, void *va){
  uint64 f = pteflags(va);
  if((long)f == -1){
    printf("%s: flags=UNMAPPED\n", tag);
    return;
  }
  char buf[12];
  fmt_flags(f, buf);
  printf("%s: flags=%s\n", tag, buf);
}

// Result helpers. Each helper runs in a child process
// and reports success/failure via exit status.
static int child_read_ok(char *p){
  int pid = fork(); if(pid < 0) die("fork failed");
  if(pid == 0){ volatile char x = p[0]; (void)x; exit(0); }
  int st=0; wait(&st); return st == 0;
}
static int child_read_blocked(char *p){
  return !child_read_ok(p);
}
static int child_write_ok(char *p){
  int pid = fork(); if(pid < 0) die("fork failed");
  if(pid == 0){ p[0] ^= 1; exit(0); }
  int st=0; wait(&st); return st == 0;
}
static int child_write_blocked(char *p){
  return !child_write_ok(p);
}
static int child_exec_ok(void (*fn)(void)){
  int pid = fork(); if(pid < 0) die("fork failed");
  if(pid == 0){ fn(); exit(0); }
  int st=0; wait(&st); return st == 0;
}
static int child_read_u8_ok(void *addr){
  int pid = fork(); if(pid < 0) die("fork failed");
  if(pid == 0){ volatile unsigned char b = *(unsigned char*)addr; (void)b; exit(0); }
  int st=0; wait(&st); return st == 0;
}

// ---------- Minimal execute stub page ----------
// Allocate a fresh page and write a single "ret" instruction (0x00008067).
// This keeps EXEC tests isolated from the current text page.
static void (*make_exec_stub(void))(void){
  unsigned char *page = (unsigned char *)sbrk(PGSIZE);
  if(page == (unsigned char*)-1) return 0;
  page[0] = 0x67; page[1] = 0x80; page[2] = 0x00; page[3] = 0x00; // ret
  for(int i=4;i<PGSIZE;i++) page[i] = 0;
  return (void (*)(void)) (page);
}

int
main(void)
{
  // Use a misaligned [addr,len] so the kernel must round to page boundaries internally.
  char *data = sbrk(2*PGSIZE);
  if(data == (char*)-1) die("sbrk failed");
  for(int i=0;i<2*PGSIZE;i++) data[i] = 'A';

  char *mid = data + 500;
  uint64 len = 5000;

  // ===== A) Data range tests =====
  printf("== DATA ==\n");
  show_flags("data:init @mid",  mid);
  printf("\n");

  // [1] PROT_READ: read allowed, write must fault
  passfail("[1] set PROT_READ", mprotect(mid, len, PROT_READ) == 0);
  show_flags("data:PROT_R @mid", mid);
  passfail("[1.1] read ok",     child_read_ok(mid));
  passfail("[1.2] write fault", child_write_blocked(mid));
  printf("\n");

  // [2] PROT_NONE: both read and write must fault
  passfail("[2] set PROT_NONE", mprotect(mid, len, PROT_NONE) == 0);
  show_flags("data:NONE  @mid", mid);
  passfail("[2.1] read blocked",  child_read_blocked(mid));
  passfail("[2.2] write blocked", child_write_blocked(mid));
  printf("\n");

  // [3] PROT_READ|PROT_WRITE: both read and write should succeed
  passfail("[3] set PROT_RW", mprotect(mid, len, PROT_READ | PROT_WRITE) == 0);
  show_flags("data:PROT_RW@mid", mid);
  passfail("[3.1] read ok (RW)",  child_read_ok(mid));
  passfail("[3.2] write ok (RW)", child_write_ok(mid));
  printf("\n");

  // ===== B) Execute permission tests on an isolated stub page =====
  printf("== EXEC ==\n");
  void (*stub)(void) = make_exec_stub();
  if(stub == 0) die("make_exec_stub failed");
  void *stub_page = (void*) ((uint64)stub & ~(PGSIZE-1));
  show_flags("exec:init @stub", stub);
  printf("\n");

  // [4] PROT_NONE: execution must fault
  passfail("[4] stub PROT_NONE", mprotect(stub_page, PGSIZE, PROT_NONE) == 0);
  show_flags("exec:NONE  @stub", stub);
  passfail("[4.1] call blocked",  !child_exec_ok(stub));
  printf("\n");

  // [5] PROT_EXEC: execution allowed, reading should fault
  passfail("[5] stub PROT_EXEC", mprotect(stub_page, PGSIZE, PROT_EXEC) == 0);
  show_flags("exec:X     @stub", stub);
  passfail("[5.1] call ok",       child_exec_ok(stub));
  passfail("[5.2] read blocked", !child_read_u8_ok(stub));
  printf("\n");

  // [6] PROT_READ: reading allowed, execution must fault
  passfail("[6] stub PROT_READ", mprotect(stub_page, PGSIZE, PROT_READ) == 0);
  show_flags("exec:R     @stub", stub);
  passfail("[6.1] read ok",       child_read_u8_ok(stub));
  passfail("[6.2] call blocked", !child_exec_ok(stub));
  printf("\n");

  // [7] PROT_READ|PROT_EXEC: both read and execute allowed
  passfail("[7] stub PROT_RX", mprotect(stub_page, PGSIZE, PROT_READ | PROT_EXEC) == 0);
  show_flags("exec:RX    @stub", stub);
  passfail("[7.1] call ok", child_exec_ok(stub));
  printf("\n");

  printf("== ALL mprotect CHECKS PASSED ==\n");
  exit(0);
}


#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// return number of free physical pages.
uint64
sys_freepages(void)
{
  return kfreepages();
}

// return PTE flags of a given user virtual address.
uint64
sys_pteflags(void)
{
  uint64 va;
  argaddr(0, &va);

  struct proc *p = myproc();
  va = PGROUNDDOWN(va);

  // check if valid user mapping
  pte_t *pte = walk(p->pagetable, va, 0);
  if(pte == 0)              return -1;
  if((*pte & PTE_V) == 0)   return -1;
  if((*pte & PTE_U) == 0)   return -1;

  return PTE_FLAGS(*pte);
}

// return physical address of a given user virtual address.
uint64
sys_ptepa(void)
{
  uint64 va;
  argaddr(0, &va);

  struct proc *p = myproc();
  uint64 va0 = PGROUNDDOWN(va);

  // check if valid user mapping
  pte_t *pte = walk(p->pagetable, va0, 0);
  if(pte == 0)            return -1;
  if((*pte & PTE_V) == 0) return -1;
  if((*pte & PTE_U) == 0) return -1;

  // physical address = PTE base + page offset
  uint64 pa = PTE2PA(*pte) + (va & (PGSIZE-1));
  return pa;
}


// change memory protection of a given range
uint64
sys_mprotect(void)
{
  uint64 addr, len;
  int prot;
  argaddr(0, &addr); // starting address
  argaddr(1, &len);  // length in bytes
  argint(2, &prot);  // protection flags

  struct proc *p = myproc();

  if(addr + len < addr) return -1; // overflow
  if(addr + len > p->sz) return -1; // out of process memory range

  return vm_mprotect(p->pagetable, addr, len, prot);
}

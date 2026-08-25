// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int free_pages;
} kmem;

// 물리 페이지별 참조 카운트 테이블
static struct {
  struct spinlock lock;
  int count[PHYSTOP / PGSIZE];
} refcnt;

static inline int
pa2idx(uint64 pa)
{
  return pa / PGSIZE;
}

// uvmcopy()에서 parent/child가 페이지를 공유하게 될 때 호출
void
krefinc(void *pa)
{
  acquire(&refcnt.lock);
  refcnt.count[pa2idx((uint64)pa)]++;
  release(&refcnt.lock);
}

int
kgetrefc(void *pa)
{
  int c;
  acquire(&refcnt.lock);
  c = refcnt.count[pa2idx((uint64)pa)];
  release(&refcnt.lock);
  return c;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcnt.lock, "refcnt");
  kmem.free_pages = 0;
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    refcnt.count[pa2idx((uint64)p)] = 1;   // kfree가 감소시킬 수 있도록 선세팅
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int idx;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  idx = pa2idx((uint64)pa);

  acquire(&refcnt.lock);
  if(refcnt.count[idx] < 1)
    panic("kfree: refcount underflow");
  refcnt.count[idx]--;
  if(refcnt.count[idx] > 0){
    release(&refcnt.lock);
    return;
  }
  release(&refcnt.lock);

  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE);
    acquire(&refcnt.lock);
    refcnt.count[pa2idx((uint64)r)] = 1;
    release(&refcnt.lock);
  }
  return (void*)r;
}

// Return the number of free physical pages.
int
kfreepages(void)
{
  int n;
  acquire(&kmem.lock);
  n = kmem.free_pages;
  release(&kmem.lock);
  return n;
}

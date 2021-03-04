// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"



extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
  uint64 nlists;
};

struct kmem kmems[NCPU];

void freerange(void *pa_start, void *pa_end, struct kmem *pkmem);
void _kfree(void *pa, struct kmem *pkmem);
void * _kalloc(struct kmem *pkmem);

void
kinit()
{
  char *q;
  char *p  = (char*)PGROUNDUP((uint64)end);
  int pages = ((char *)PHYSTOP - p) / PGSIZE;
  int pagesPerCPU = pages / NCPU;

  for(int i=0; i<NCPU; ++i) {
    initlock(&kmems[i].lock, "kmem");
    kmems[i].nlists = 0;
    if(i == NCPU - 1) q = (char *)PHYSTOP;
    else q = p + pagesPerCPU * PGSIZE; 
    freerange(p, q, &kmems[i]);
    p = q;
  }
 
}

void
freerange(void *pa_start, void *pa_end, struct kmem *pkmem)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    _kfree(p, pkmem);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  _kfree(pa, &kmems[cpuid()]);
}

void
_kfree(void *pa, struct kmem *pkmem)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&pkmem->lock);
  r->next = pkmem->freelist;
  pkmem->freelist = r;
  pkmem->nlists += 1;
  release(&pkmem->lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc()
{
  push_off();
  int idx = cpuid();
  pop_off();
  if(kmems[cpuid()].nlists == 0) {
    for(int i=NCPU-1; i>=0; --i)
    {
      if(kmems[i].nlists > 0) {
        idx = i;
        break;
      }
    }
  }
  return _kalloc(&kmems[idx]);
}

void *
_kalloc(struct kmem *pkmem)
{
  struct run *r;

  acquire(&pkmem->lock);
  r = pkmem->freelist;
  if(r){
    pkmem->nlists -= 1;
    pkmem->freelist = r->next;
  }
  release(&pkmem->lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

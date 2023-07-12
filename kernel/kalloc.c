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
void dec_refcount(void* pa);
void inc_refcount(void* pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int page_refcount[PHYSTOP / PGSIZE];
} page_refstruct;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&page_refstruct.lock, "pgref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    page_refstruct.page_refcount[(uint64)p/PGSIZE] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Only free the page when ref is 0
  uint64 page = (uint64)pa / PGSIZE;
  dec_refcount(pa);
  acquire(&page_refstruct.lock);
  if (page_refstruct.page_refcount[page] > 0) {
    release(&page_refstruct.lock);
    return;
  } else if (page_refstruct.page_refcount[page] < 0) {
    panic("page ref count should not be negative");
  } else {
    // Fill with junk to catch dangling refs.
    release(&page_refstruct.lock);
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
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

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  // set page ref count to 1
  acquire(&page_refstruct.lock);
  page_refstruct.page_refcount[(uint64)r / PGSIZE] = 1;
  release(&page_refstruct.lock);
  return (void*)r;
}

void
inc_refcount(void* pa)
{
  acquire(&page_refstruct.lock);
  uint64 page = (uint64)pa / PGSIZE;
  page_refstruct.page_refcount[page] += 1;
  release(&page_refstruct.lock);
}

void
dec_refcount(void* pa)
{
  uint64 page = (uint64)pa / PGSIZE;

  acquire(&page_refstruct.lock);
  page_refstruct.page_refcount[page] -= 1;

  if (page_refstruct.page_refcount[page] < 0)
    panic("dec_refcount: negative refcount\n");
  release(&page_refstruct.lock);
}

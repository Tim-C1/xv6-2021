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

struct kmem_list_percpu {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem_list_percpu kmem_lists[NCPU];

struct kmem_list_percpu* get_current_kmem_off() {
  push_off();
  return &kmem_lists[cpuid()];
}

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem_lists[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  struct kmem_list_percpu* kmem = get_current_kmem_off();
  acquire(&kmem->lock);
  r->next = kmem->freelist;
  kmem->freelist = r;
  release(&kmem->lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  struct kmem_list_percpu* kmem = get_current_kmem_off();
  acquire(&kmem->lock);
  r = kmem->freelist;
  if(r) {
    kmem->freelist = r->next;
  } else {
    release(&kmem->lock);
    for (int i = 0; i < NCPU; i++) {
      push_off();
      if (cpuid() == i) {
        pop_off();
        continue;
      }
      pop_off();
      struct kmem_list_percpu* kmem_i = &kmem_lists[i];
      acquire(&kmem_i->lock);
      r = kmem_i->freelist;
      if (r) {
        kmem_i->freelist = r->next;
        release(&kmem_i->lock);
        if (r)
          memset((char *)r, 5, PGSIZE);
        break;
      }
      release(&kmem_i->lock);
    }
    pop_off();
    return (void *)r;
  }
  release(&kmem->lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}

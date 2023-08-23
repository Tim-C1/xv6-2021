// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(bno) bno % NBUCKET

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf buckets[NBUCKET];
  struct spinlock bucket_locks[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucket_locks[i], "bcache.bucket");
    // use to test if we reach the end
    bcache.buckets[i].next = 0;
  }
  // give all the bufs to bucket0
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    b->lastuse_ticks = 0;
    b->refcnt = 0;
    b->next = bcache.buckets[0].next;
    bcache.buckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *b_lru = 0;
  struct buf *b_lru_pre = 0;
  uint least_ticks = -1;

  // Is the block already cached?
  uint id = HASH(blockno);
  acquire(&bcache.bucket_locks[id]);
  for (b = bcache.buckets[id].next; b != 0; b = b->next) {
    if (b->blockno == blockno && b->dev == dev) {
      b->refcnt++;
      release(&bcache.bucket_locks[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_locks[id]);
  // Not cached.
  // Find the least recently used buffer in all buckets
  acquire(&bcache.lock);
  for (int i = 0; i < NBUCKET; i++) {
    struct buf *pre = &bcache.buckets[i];
    // maybe another process has already evinct a new buffer for this bucket when we release the bucket lock, so we check again
    if (i == id) {
      for (b = bcache.buckets[id].next; b != 0; b = b->next) {
        if (b->blockno == blockno && b->dev == dev) {
          acquire(&bcache.bucket_locks[id]);
          b->refcnt++;
          release(&bcache.bucket_locks[id]);
          release(&bcache.lock);
          acquiresleep(&b->lock);
          return b;
        }
      }
    }
    for (b = bcache.buckets[i].next; b != 0; b = b->next) {
      if (b->refcnt == 0 && b->lastuse_ticks < least_ticks) {
        b_lru = b;
        b_lru_pre = pre;
        least_ticks = b->lastuse_ticks;
      }
      pre = pre->next;
    }
  }
  
  if (b_lru) {
    // move b_lru to current bucket
    if (!(b_lru_pre->next == b_lru)) {
      panic("opps");
    }
    b_lru_pre->next = b_lru_pre->next->next;
    b_lru->next = bcache.buckets[id].next;
    bcache.buckets[id].next = b_lru;

    b_lru->dev = dev;
    b_lru->blockno = blockno;
    b_lru->valid = 0;
    b_lru->refcnt = 1;
    release(&bcache.lock);
    acquiresleep(&b_lru->lock);
    return b_lru;
  } else {
    panic("bget: no buffers");
  }
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  uint id = HASH(b->blockno);
  acquire(&bcache.bucket_locks[id]);
  b->refcnt--;
  if (b->refcnt == 0)
    b->lastuse_ticks = ticks;
  release(&bcache.bucket_locks[id]);
}

void
bpin(struct buf *b) {
  uint id = HASH(b->blockno);
  acquire(&bcache.bucket_locks[id]);
  b->refcnt++;
  release(&bcache.bucket_locks[id]);
}

void
bunpin(struct buf *b) {
  uint id = HASH(b->blockno);
  acquire(&bcache.bucket_locks[id]);
  b->refcnt--;
  release(&bcache.bucket_locks[id]);
}



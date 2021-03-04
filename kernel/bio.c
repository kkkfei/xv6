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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

struct buf bufhashtable[NBUCKET];
struct spinlock bucketLocks[NBUCKET];

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i=0; i<NBUCKET; ++i)
  {
    initlock(&bucketLocks[i], "bcache");
    bufhashtable[i].next = bufhashtable[i].prev = &bufhashtable[i];
  }

  struct buf *h = &bufhashtable[0];
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
 
    initsleeplock(&b->lock, "buffer");

    b->prev = h;
    b->next = h->next;
    h->next->prev = b;
    h->next = b;
  }  
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *h;
  struct buf *p;
  uint hashIdx = blockno % NBUCKET;
  h = &bufhashtable[hashIdx];

  acquire(&bucketLocks[hashIdx]);

  for(p=h->next; p != h; p = p->next)
  {
    if(p->dev == dev && p->blockno == blockno) {
      p->refcnt ++;
      release(&bucketLocks[hashIdx]);
      acquiresleep(&p->lock);
 
      return p;
    }
  }
  release(&bucketLocks[hashIdx]);


  // do eviction
  acquire(&bcache.lock);
  acquire(&bucketLocks[hashIdx]);

  //double check
  for(p=h->next; p != h; p = p->next)
  {
    if(p->dev == dev && p->blockno == blockno) {
      p->refcnt ++;
      release(&bucketLocks[hashIdx]);
      release(&bcache.lock);
      acquiresleep(&p->lock);
      return p;
    }
  }
 
 
  struct buf *b = 0;
  uint lruticks = 0;
  uint oldhashidx = NBUCKET;
  uint found = 0;

  for(int i=0; i<NBUCKET; ++i) {
    if(i != hashIdx) acquire(&bucketLocks[i]);
    struct buf *t = &bufhashtable[i];
    found = 0;
    for(p=t->next; p != t; p = p->next)
    {
      if(p->refcnt == 0) {
        if(b == 0 || p->lastticks < lruticks) {
          if(b!=0 && oldhashidx != i && oldhashidx != hashIdx) release(&bucketLocks[oldhashidx]);

          b = p;
          lruticks = p->lastticks;
          found = 1;
          oldhashidx = i;
        } 
      }
    }
    if(found == 0 && i != hashIdx)  release(&bucketLocks[i]);
  }

  if(b == 0) panic("bget error!");

  if(oldhashidx != hashIdx) {
    b->prev->next = b->next;
    b->next->prev = b->prev;
    release(&bucketLocks[oldhashidx]);
    b->next = h->next;
    b->prev = h;
    h->next->prev = b;
    h->next = b;
  } 


  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bucketLocks[hashIdx]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
 
  return b;
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

  uint hashIdx = b->blockno % NBUCKET;
  releasesleep(&b->lock);
  acquire(&bucketLocks[hashIdx]);
 
  b->refcnt--;
  if(b->refcnt == 0) {
    acquire(&tickslock);
    b->lastticks = ticks; 
    release(&tickslock);
  }
  release(&bucketLocks[hashIdx]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}



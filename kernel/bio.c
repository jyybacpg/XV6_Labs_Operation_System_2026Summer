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

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock evict_lock;
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKET];
} bcache;

static uint
bhash(uint dev, uint blockno)
{
  return (dev ^ blockno) % NBUCKET;
}

void
binit(void)
{
  struct buf *b;
  struct bucket *bk;

  initlock(&bcache.evict_lock, "bcache_evict");

  for(int i = 0; i < NBUCKET; i++){
    bk = &bcache.bucket[i];
    initlock(&bk->lock, "bcache");
    bk->head.prev = &bk->head;
    bk->head.next = &bk->head;
  }

  // Put all free buffers on one bucket initially. They move to their
  // hashed bucket when bget assigns a device and block number.
  bk = &bcache.bucket[0];
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->next = bk->head.next;
    b->prev = &bk->head;
    bk->head.next->prev = b;
    bk->head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct bucket *bk;
  uint h;

  h = bhash(dev, blockno);
  bk = &bcache.bucket[h];

  // Is the block already cached?
  acquire(&bk->lock);
  for(b = bk->head.next; b != &bk->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bk->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bk->lock);

  acquire(&bcache.evict_lock);

  // Recheck while holding the eviction lock; another CPU may have
  // inserted this block after the first lookup missed.
  acquire(&bk->lock);
  for(b = bk->head.next; b != &bk->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bk->lock);
      release(&bcache.evict_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bk->lock);

  // Not cached.
  // Recycle an unused buffer from any bucket.
  for(int i = 0; i < NBUCKET; i++){
    struct bucket *old = &bcache.bucket[i];

    acquire(&old->lock);
    for(b = old->head.prev; b != &old->head; b = b->prev){
      if(b->refcnt == 0) {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&old->lock);

        acquire(&bk->lock);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->next = bk->head.next;
        b->prev = &bk->head;
        bk->head.next->prev = b;
        bk->head.next = b;
        release(&bk->lock);

        release(&bcache.evict_lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&old->lock);
  }
  panic("bget: no buffers");
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
  struct bucket *bk;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  bk = &bcache.bucket[bhash(b->dev, b->blockno)];
  acquire(&bk->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is using it; move it to the MRU position in this bucket.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bk->head.next;
    b->prev = &bk->head;
    bk->head.next->prev = b;
    bk->head.next = b;
  }
  
  release(&bk->lock);
}

void
bpin(struct buf *b) {
  struct bucket *bk = &bcache.bucket[bhash(b->dev, b->blockno)];
  acquire(&bk->lock);
  b->refcnt++;
  release(&bk->lock);
}

void
bunpin(struct buf *b) {
  struct bucket *bk = &bcache.bucket[bhash(b->dev, b->blockno)];
  acquire(&bk->lock);
  b->refcnt--;
  release(&bk->lock);
}



// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "memlayout.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb;

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for(b = 0; b < sb.size; b += BPB){ //SWAP: sb.size -> sb.swapsize
    bp = bread(dev, BBLOCK(b, sb)); //bread takes a blockno as a param.
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

//Swap Pages Available
//Number of pages we can fit in swap space. 1000/(4096/512), or 1000/8, or 125.
//Yes, the name sucks. I'm sorry.
#define SPA (SWAPSIZE/(PGSIZE/BSIZE))

struct {
  struct spinlock swaplock;
  uint present[SPA];
} swaptable;

void initswap(void)
{
	initlock(&swaptable.swaplock, "swaptable");
}

//Writes a page of memory to the swap space.
//Does not do pageTable housecleaning. Do that yourself.
//PGSIZE/BSIZE gets us the number of blocks per page
//Returns a page number upon success.
//Returns -1 on failure;
int writePage(uint dev, uint va_page)
{
	int pageNumber, pageOffset, i, j;
	struct buf* block;

	//Select open page number for processing.
	acquire(&swaptable.swaplock);
// 	cprintf("\n[L] writePage has acquired a lock. Number of active locks is: %d\n", cpu->ncli);
	for(pageNumber = 0; pageNumber < SPA; pageNumber++)
	{
		if(!swaptable.present[pageNumber]) //If the swap table indicates that this page hasn't been allocated
		{
			swaptable.present[pageNumber] = 1; //Mark as used
			break;
		}
	}
	if(pageNumber >= SPA)
	{
		cprintf("[X] writePage: global swap space is full.\n");
		release(&swaptable.swaplock);
		return -1;
	}

	//Find an open slot in tracking arrays to store the evicted va and page number it's stored in.
	for(i = 0; i < MAX_SWAP_PAGES; i++)
	{
		if(proc->swap_stored_va[i] == 0xFFFFFFFF) //open slot
		{
			proc->swap_stored_va[i] = va_page; //remember to store the swap page number in proc->swap_page_numbers
			break;
		}
	}
	if(i == MAX_SWAP_PAGES)
	{
		cprintf("[X] writePage: swap file quota exceeded for this process.\n");
		swaptable.present[pageNumber] = 0; //Mark as unused
		release(&swaptable.swaplock);
		return -1;
	}
	proc->swap_page_numbers[i] = pageNumber;

	//pageOffset is the actual location of the page in question
	pageOffset = pageNumber*(BSIZE/PGSIZE);

	release(&swaptable.swaplock);

	//Should execute 4 times: 4 blocks per page.
	begin_op();

	for(j = 0; j < PGSIZE/BSIZE; j++)
	{
		block = bread(dev, sb.swapstart+pageOffset+j);
		memmove(block->data, (char*)va_page+(BSIZE*j), BSIZE); //Here is the magic. Copy one page from memory to buffer/block struct.
		bwrite(block);
		brelse(block);
	}
	end_op();

	proc->pagesInMemory--;
	proc->pagesInSwap++;

	cprintf("[W] Pid %d: %d pages in memory, %d pages in swap.\n", proc->pid, proc->pagesInMemory, proc->pagesInSwap);

	return pageNumber;
}

int removeProcFromSwap(void)
{
	int i;

	acquire(&swaptable.swaplock);
	for(i = 0; i < MAX_SWAP_PAGES; i++)
	{
		if(proc->swap_page_numbers[i] != 0xFFFFFFFF)
		{
			swaptable.present[i] = 0; //just toombstone the entry.
		}
	}
	release(&swaptable.swaplock);

	return 0;
}

//Must pass a va AFTER you call pagerounddown macro.
int trackMemPage(uint va)
{
	int i;

	//Find first open slot
	acquire(&swaptable.swaplock);
	for(i = 0; i < MAX_RAM_PAGES; i++)
	{
		if (proc->ram_pages[i] == 0xFFFFFFFF)
		{
			break;
		}
	}
	release(&swaptable.swaplock);
	if (i == MAX_RAM_PAGES)
	{
		panic("trackMemPage: memory full trying to add to ram_pages.\n");
	}

	//write stamps at chosen slot
	acquire(&tickslock);
	proc->fifoTimestamps[i] = ticks;
	release(&tickslock);

	proc->pagesInMemory++;

	proc->ram_pages[i] = va;
	proc->NfuPageAges[i] = (1 << 31);

// 	cprintf("[V] Tracking va %p. Timestamp is %d.\n", va, proc->fifoTimestamps[i]);

	return i;
}

int admit(uint va)
{
	pte_t *pte;
	int i, j;
	char* mem;
	int pageOffset;
	int pageNumber;
	struct buf* block;

	if(va == 0)
	{
		cprintf("[X] admit: passed a va of zero.\n");
		return -1;
	}
	else
	{
		cprintf("[V] admit: Attempting to admit va address of %p.\n", va);
	}
	va = PGROUNDDOWN(va);
	if(va <= 0)
	{
		cprintf("[X] admit: PGROUNDOWN has resulted in addressing page %d.\n", va);
		return -1;
	}

	acquire(&swaptable.swaplock);

	for(i = 0; i < MAX_SWAP_PAGES; i++)
	{
		if(proc->swap_stored_va[i] == va) //if our table says it's in swap
		{
			proc->swap_stored_va[i] = 0xFFFFFFFF; //set it to unused.
			break;
		}
	}
	if(i == MAX_SWAP_PAGES)
	{
		panic("admit: page va was not found in swap.");
	}

	mem = kalloc();
	if(mem == 0)
	{
		panic("admit: could not kalloc.");
	}
	memset(mem, 0, PGSIZE); //Zero the memory region. probably not needed.
	if ((pte = walkpgdir(proc->pgdir,(char*)va,0)) == 0)
	{
		panic("admit: walkpgdir failure.");
	}

	pageNumber = proc->swap_page_numbers[i];
	pageOffset = pageNumber * (PGSIZE/BSIZE);

	release(&swaptable.swaplock);

	begin_op();
	cprintf("[O] Begin read operation...\n");
	for(j = 0; j < PGSIZE/BSIZE; j++)
	{
		cprintf("[O] Attempting to read page %d of %d.\n", j+1, PGSIZE/BSIZE);
		block = bread(ROOTDEV, sb.swapstart+pageOffset+j); //Get the block we need.
		cprintf("[O] Obtained block pointer. Moving to va %p.\n", va);
		memmove((uchar*)(va+(BSIZE*j)), block->data, sizeof(block->data)); //Here is the magic. Copy one page from buffer/block struct to memory.
		cprintf("[O] Memory block modified.\n");
		swaptable.present[pageNumber] = 0; //Just toombstone the table entry instead of making 4 writes to disk.
		brelse(block);
		cprintf("[O] Read of page %d of %d complete.\n", j+1, PGSIZE/BSIZE);
	}
	end_op();
	cprintf("[O] Operation complete. Page has been read.\n");

	*pte &= 0xFFF;
	*pte |= V2P(mem);
	*pte &= (~PTE_PG);
	*pte |= PTE_P;
	proc->pagesInSwap--;
	proc->swap_page_numbers[i] = 0xFFFFFFFF;
	cprintf("[M] Page has been read. %d pages in memory, %d pages in swap.\n", proc->pagesInMemory, proc->pagesInSwap);

	trackMemPage(va); //VA has been rounded already.

	return 0;
}

//Evicts a page from memory.
//Which page is decided by a chosen algorithm: FIFO or NFU.
//A return of -1 indicates that swapping was aborted.
uint evict(pde_t* pageDirectory)
{
	uint va_page = 0xFFFFFFFF;
	pde_t *pte;

	#ifdef NONE
	return -1;
	#endif

	//Don't swap anything from init or sh
	if(proc->pid <= 2)
	{
		return -1;
	}
	if(proc->pagesInSwap > MAX_SWAP_PAGES)
	{
		panic("[X] evict: swap is full for this process.\n");
	}

	acquire(&swaptable.swaplock);
	#ifdef FIFO
	va_page = getFifoPage();
	#endif
	#ifdef NFU
	va_page = getOldNfuPage();
	#endif
	release(&swaptable.swaplock);

	if(va_page == 0xFFFFFFFF)
	{
		panic("[X] evict: Attempted to evict va_page 0xFFFFFFFF.\n");
	}
	if(writePage(ROOTDEV, va_page) < 0)
	{
		panic("evict: failure during write to swap.\n");
	}
	if ((pte = walkpgdir(pageDirectory,(char*)va_page,0)) == 0)
	{
		panic("evict: walkpgdir failure.");
	}

	*pte &= ~PTE_P; //Remove present bit
	*pte |= PTE_PG; //Indicate this page has been paged out, not removed.
	kfree(P2V(PTE_ADDR(*pte)));

	return 0;
}

// Free a disk block.
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  readsb(dev, &sb);
  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->flags.
//
// An inode and its in-memory represtative go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, iput() frees if
//   the link count has fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() to find or
//   create a cache entry and increment its ref, iput()
//   to decrement ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when the I_VALID bit
//   is set in ip->flags. ilock() reads the inode from
//   the disk and sets I_VALID, while iput() clears
//   I_VALID if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void
iinit(int dev)
{
  int i = 0;

  initlock(&icache.lock, "icache");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }

  readsb(dev, &sb);
  cprintf("\n[BOOT] Superblock:\n");
  cprintf("[BOOT] Size of file system:                 %d\n", sb.size);
  cprintf("[BOOT] Number of data blocks:               %d\n", sb.nblocks);
  cprintf("[BOOT] Number of inodes                     %d\n", sb.ninodes);
  cprintf("[BOOT] Number of log blocks                 %d\n", sb.nlog);
  cprintf("[BOOT] Number of swap blocks                %d\n", sb.swapblocks);
  cprintf("[BOOT] Block number of first log block      %d\n", sb.logstart);
  cprintf("[BOOT] Block number of first inode block    %d\n", sb.inodestart);
  cprintf("[BOOT] Block number of first swap block     %d\n", sb.swapstart);
  cprintf("[BOOT] Block number of first free map block %d\n\n", sb.bmapstart);
}

static struct inode* iget(uint dev, uint inum);

//PAGEBREAK!
// Allocate a new inode with the given type on device dev.
// A free inode has a type of zero.
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}

// Copy a modified in-memory inode to disk.
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->flags = 0;
  release(&icache.lock);

  return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(!(ip->flags & I_VALID)){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->flags |= I_VALID;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
iput(struct inode *ip)
{
  acquire(&icache.lock);
  if(ip->ref == 1 && (ip->flags & I_VALID) && ip->nlink == 0){
    // inode has no links and no other references: truncate and free.
    release(&icache.lock);
    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    acquire(&icache.lock);
    ip->flags = 0;
  }
  ip->ref--;
  release(&icache.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

//PAGEBREAK!
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// Copy stat information from inode.
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

//PAGEBREAK!
// Read data from inode.
int
readi(struct inode *ip, char *dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(ip, dst, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    /*
    cprintf("data off %d:\n", off);
    for (int j = 0; j < min(m, 10); j++) {
      cprintf("%x ", bp->data[off%BSIZE+j]);
    }
    cprintf("\n");
    */
    memmove(dst, bp->data + off%BSIZE, m);
    brelse(bp);
  }
  return n;
}

// PAGEBREAK!
// Write data to inode.
int
writei(struct inode *ip, char *src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;
    return devsw[ip->major].write(ip, src, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(bp->data + off%BSIZE, src, m);
    log_write(bp);
    brelse(bp);
  }

  if(n > 0 && off > ip->size){
    ip->size = off;
    iupdate(ip);
  }
  return n;
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(proc->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}

//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((LOGSIZE-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

//Moves/sets the offset pointer within a file.
//Only ever used in evict/admit pages function.
void setSwapFileOffset(struct file* swapFile, uint offset)
{
	swapFile->off = offset;
}

//only ever used during fork
int readSwapFileAtOffset(struct file *swapFile, char *addr, int n, int offset)
{
	if(swapFile->readable == 0)
	{
		cprintf("readSwapFileAtOffset: Not a readable swapfile.\n");
		return -1;
	}
	if(swapFile->type == FD_PIPE)
	{
		cprintf("readSwapFileAtOffset: Trying to read swap from a pipe.\n");
		return piperead(swapFile->pipe, addr, n);
	}
	if(swapFile->type == FD_INODE)
	{
		ilock(swapFile->ip);
		readi(swapFile->ip, addr, offset, n);
		iunlock(swapFile->ip);

		return n;
	}

  panic("readSwapFileAtOffset: unknown filetype.");
}

//only ever used during fork
int writeSwapFileAtOffset(struct file *swapFile, char *addr, int n, int offset)
{
    int r;

	if(swapFile->writable == 0)
	{
		cprintf("writeSwapFileAtOffset: Not a writable swap file.\n");
		return -1;
	}

	if(swapFile->type == FD_PIPE)
	{
		cprintf("writeSwapFileAtOffset: Trying to write swapfile to a pipe.\n");
		return pipewrite(swapFile->pipe, addr, n);
	}

	if(swapFile->type == FD_INODE)
	{
		//Code borrowed from filewrite().
		int max = ((LOGSIZE-1-1-2) / 2) * 512;
		int i = 0;
		begin_op(); //This being up here may or may not cause a slowdown.
		while(i < n)
		{
			int n1 = n - i;
			if(n1 > max)
			{
				n1 = max;
			}

			ilock(swapFile->ip);
			if((r = writei(swapFile->ip, addr + i, offset, n1)) > 0)
			{
				offset+=r; //experiment with this being swapFile->off += r; ?
			}
			iunlock(swapFile->ip);

			if(r < 0)
			{
				break;
			}
			if(r != n1)
			{
				panic("writeSwapFileAtOffset: short writeSwapFileAtOffset");
			}
			i += r;
		}

		end_op();
		return i == n ? n : -1;
	}

	panic("writeSwapFileAtOffset: unknown filetype");
}

struct file* createSwapFile(char* path)
{
	struct inode *inodePtr;
	struct file *swapFile;

	begin_op();

	inodePtr = create(path, T_FILE, 0, 0); //ip comes back with a lock on it.
	if(inodePtr == 0)
	{
		cprintf("createSwapFile: Failure in createSwapFile, could not create inode.\n");
		return (struct file*)-1;
	}

	swapFile = filealloc();
	if(swapFile == 0)
	{
		cprintf("createSwapFile: Failed to create a file pointer in createSwapFile.\n");
		fileclose(swapFile);
		iunlockput(inodePtr);
		return (struct file*)-1;
	}

	swapFile->type = FD_INODE;	//This file is of type inode
	swapFile->ip = inodePtr; 	//This file is actually just this inode
	swapFile->off = 0; 			//Pointer to top of file
	swapFile->readable = 1; 	//Is readable
	swapFile->writable = 1; 	//Is writable
	iunlock(inodePtr);
	end_op();

	return swapFile;
}

//This is just a recycled version of sys_unlink.
int removeSwapFile(char* path)
{
	struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ];// *path;
  uint off;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

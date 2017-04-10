#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

#if defined(NFU) || defined(FIFO)
  int memoryPages = proc->memoryPages;
  int swappedPages = proc->swappedPages;
  int faultCount = proc->faultCount;
  int swapCount = proc->swapCount;
  struct page pages[MAX_PSYC_PAGES];
  struct pagedescriptor sPages[MAX_PSYC_PAGES];
  for (i = 0; i < MAX_PSYC_PAGES; i++) {
    pages[i].virtualAddress = proc->pages[i].virtualAddress;
    proc->pages[i].virtualAddress = (char*)0xffffffff;
    pages[i].next = proc->pages[i].next;
    proc->pages[i].next = 0;
    pages[i].previous = proc->pages[i].previous;
    proc->pages[i].previous = 0;
    pages[i].pageAge = proc->pages[i].pageAge;
    proc->pages[i].pageAge = 0;
    sPages[i].pageAge = proc->sPages[i].pageAge;
    proc->sPages[i].pageAge = 0;
    sPages[i].virtualAddress = proc->sPages[i].virtualAddress;
    proc->sPages[i].virtualAddress = (char*)0xffffffff;
    sPages[i].swapfileLocation = proc->sPages[i].swapfileLocation;
    proc->sPages[i].swapfileLocation = 0;
  }
  struct page *head = proc->head;
  struct page *tail = proc->tail;
  proc->memoryPages = 0;
  proc->swappedPages = 0;
  proc->faultCount = 0;
  proc->swapCount = 0;
  proc->head = 0;
  proc->tail = 0;
#endif
  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(proc->name, last, sizeof(proc->name));

  // Commit to the user image.
  oldpgdir = proc->pgdir;
  proc->pgdir = pgdir;
  proc->sz = sz;
  proc->tf->eip = elf.entry;  // main
  proc->tf->esp = sp;
  deleteSwapFile( proc );//Create a new swap file for the new process
  initializeSwapFile( proc );
  switchuvm(proc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
#if defined(NFU) || defined(FIFO)
  proc->memoryPages = memoryPages;
  proc->swappedPages = swappedPages;
  proc->faultCount = faultCount;
  proc->swapCount = swapCount;
  proc->head = head;
  proc->tail = tail;
  for (i = 0; i < MAX_PSYC_PAGES; i++) {
    proc->pages[i].virtualAddress = pages[i].virtualAddress;
    proc->pages[i].next = pages[i].next;
    proc->pages[i].previous = pages[i].previous;
    proc->pages[i].pageAge = pages[i].pageAge;
    proc->sPages[i].pageAge = sPages[i].pageAge;
    proc->sPages[i].virtualAddress = sPages[i].virtualAddress;
    proc->sPages[i].swapfileLocation = sPages[i].swapfileLocation;
  }
#endif
}

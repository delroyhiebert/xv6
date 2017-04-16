#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpunum() == 0){
      acquire(&tickslock);
      ticks++;
      //TODO: update NFU ages here
      #ifdef NFU
	  if(proc->pid > 2){ //Inlined version of updateNfuAges
		int i;
		pte_t *pgtab;
		for(i=0; i < MAX_PSYC_PAGES;i++) {
		  proc->NfuPageAges[i] >>= 1; //right shift by 1
		  if ((proc->memoryPages[i] != 0x00000000) && ((pgtab = walkpgdir(proc->pgdir,(char*)proc->memoryPages[i],0)) != 0) && (*pgtab & PTE_A)) {
			*pgtab &= ~(PTE_A); //clear the bit
			proc->NfuPageAges[i] |= (1 << 31); //0x80000000 (add 1 to the MSB)
		  }
		}
	  }
      #endif
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpunum(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
	//TODO: Check to see if page fault or hard page fault. If soft, call the appropriate page into memory.
	//uint a = PGROUNDDOWN(rcr2()); //Round down to faulty page address. rcr2 is the cr2 register.

	//This is just normal lazy allocation:
	//char *mem;
	//mem = kalloc();
	//memset(mem, 0, PGSIZE);
	//mappages(proc->pgdir, (char*)a, PGSIZE, v2p(mem), PTE_W|PTE_U); //Map this new memory to the virtual addr space.

	//PTE_P is present
	//PTE_U is user page
	//PTE_W is writable
	//PTE_A is accessed
	//PTE_D is dirty
	//PTE_PG is paged.

	/* Not sure what the hell this person is doing.
	addr = rcr2();
    vaddr = &proc->pgdir[PDX(addr)];
    // cprintf("addr:0x%x vaddr:0x%x PDX:0x%x PTX:0x%x FLAGS:0x%x\n", addr, vaddr, PDX(*vaddr),PTX(*vaddr),PTE_FLAGS(*vaddr)); //TODO delete
    // cprintf("&PTE_PG:%x &PTE_P:%x\n", (((uint*)PTE_ADDR(P2V(*vaddr)))[PTX(addr)] & PTE_PG), ((((uint*)PTE_ADDR(P2V(*vaddr)))[PTX(addr)] &PTE_P)));
    if (((int)(*vaddr) & PTE_P) != 0) { // if page table isn't present at page directory -> hard page fault
      if (((uint*)PTE_ADDR(P2V(*vaddr)))[PTX(addr)] & PTE_PG) { // if the page is in the process's swap file
        // cprintf("page is in swap file, pid %d, va %p\n", proc->pid, addr); //TODO delete
        swapPages(PTE_ADDR(addr));
        ++proc->totalPageFaultCount;
        return;
      }
    }
    */

// 	acquire(&proc->memoryLock);
	if(evictPage(proc->pgdir) < 0)
	{
#if defined FIFO || NFU
		cprintf("trap: evication failure");
#endif
	}
	else
	{
		//admit(rcr2());
		admitPage(PGROUNDDOWN(rcr2()));
	}
// 	release(&proc->memoryLock);

	proc->faultCount++;
    break;

  //PAGEBREAK: 13
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpunum(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpunum(), tf->eip,
            rcr2());
    proc->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}

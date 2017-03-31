#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int sys_wait2(void)
{
  int* retime, *rutime, *stime;
  if ( argptr(0, (void*)&retime, sizeof(retime)) < 0
    || argptr(1, (void*)&rutime, sizeof(retime)) < 0
    || argptr(2, (void*)&stime,  sizeof(stime )) < 0)
    return -1;
  return wait2(retime, rutime, stime);
}

int sys_set_prio(void)
{
	int priority;
	if(argint(0, &priority) < 0)
		return 1;

	//Input validation
	if((priority > 3) || (priority < 1))
	{
		return 1;
	}

	proc->plevel = priority;

	return 0;
}

int sys_yield2(void)
{
	acquire(&ptable.lock);  //DOC: yieldlock
	proc->state = RUNNABLE;
	sched();
	release(&ptable.lock);

	return 0;
}

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

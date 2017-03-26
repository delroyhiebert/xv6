/*
* Written by: Delroy Hiebert and Tyler Gauvreau
* Comp5480 Operating Systems 2
* Dr. Rasit Eskicioglu
*/
#include "types.h"
#include "user.h"

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf(1, "Usage: sanity <n>\n");
		exit();
 	}

 	int retime, rutime, stime;
 	int cpu_retime = 0, cpu_rutime = 0, cpu_stime = 0;
	int i, j, k, n, pid;
	n = atoi(argv[1]);//fork 3*n times

	printf(1, "Beginning sanity test.\n");
	for(i = 0; i < 3*n; i++)
	{
		pid = fork();
		if( pid == 0 )//Child process
		{
			//This simulates our CPU Bound processes
			for(j = 0; j < 1000000; j++)
			{
				for(k = 0; k < 100; k++)
				{
					asm("nop");
				}
			}
			exit();
		}
		continue;
	}
	printf(1, "Begin waiting for spawned children\n");
	for (i = 0; i < 3 * n; i++)
	{
		pid = wait2(&retime, &rutime, &stime);
		cpu_retime += retime;
		cpu_rutime += rutime;
		cpu_stime  += stime;
	}
	printf(1, "Exiting sanity test.\n");
	printf(1,  "CPU Bound process summary:\n\
Average Ready Time:     %d\n\
Average Running time:   %d\n\
Average Sleep Time:     %d\n\
Average Total Run Time: %d\n", cpu_retime, cpu_rutime, cpu_stime, (cpu_retime+cpu_rutime+cpu_stime)/n);
	exit();
}
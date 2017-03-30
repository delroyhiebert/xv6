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
 	int short_retime = 0, short_rutime = 0, short_stime = 0;
 	int IO_retime = 0, IO_rutime = 0, IO_stime = 0;
	int avg_cpu_retime, avg_cpu_rutime, avg_cpu_stime;
	int avg_short_retime, avg_short_rutime, avg_short_stime;
	int avg_IO_retime, avg_IO_rutime, avg_IO_stime;
	int i, j, k, n, pid;
	n = atoi(argv[1]);//fork 3*n times
	char type[6];

	printf(1, "Beginning sanity test.\n");
	for( i = 0; i < 3*n; i++ )
	{
		pid = fork();
		if( pid == 0 )//Child process
		{
			//printf(1, "Pid %d is running.\n", getpid());
			switch(getpid() % 3)
			{
				case 0:
					//This simulates our CPU Bound processes
					for( j = 0; j < 100; j++ )
					{
						for( k = 0; k < 1000000; k++ )
						{
							asm("nop");
						}
					}
					break;
				case 1:
					for( j = 0; j < 100; j++ )
					{
						for( k = 0; k < 1000000; k++ )
						{
							asm("nop");
						}
						yield2();
					}
					break;
				case 2:
					for( j = 0; j < 100; j++ )
					{
						sleep(1);
					}
					break;
			}
			//printf(1, "Pid %d is done running.\n", getpid());
			exit();
		}
		continue;
	}
	printf(1, "Begin waiting for spawned children\n");
	for (i = 0; i < 3 * n; i++)
	{
		pid = wait2(&retime, &rutime, &stime);

		switch(pid % 3)
		{
			case 0:
				strcpy(type, "CPU\0");
				break;
			case 1:
				strcpy(type, "Short\0");
				break;
			case 2:
				strcpy(type, "I/O\0");
				break;
		}
		//printf(1, "Pid: %d, Type: %s, Ready time: %d, Run time: %d, I/O time: %d\n", pid, type, retime, rutime, stime);

		switch(pid % 3)
		{
			case 0:
				cpu_retime   += retime;
				cpu_rutime   += rutime;
				cpu_stime    += stime;
				break;
			case 1:
				short_retime += retime;
				short_rutime += rutime;
				short_stime  += stime;
				break;
			case 2:
				IO_retime    += retime;
				IO_rutime    += rutime;
				IO_stime     += stime;
				break;
		}

	}

	avg_cpu_retime = cpu_retime / n;
	avg_cpu_rutime = cpu_rutime / n;
	avg_cpu_stime  = cpu_stime  / n;
	avg_short_retime = short_retime / n;
	avg_short_rutime = short_rutime / n;
	avg_short_stime = short_stime / n;
	avg_IO_retime = IO_retime / n;
	avg_IO_rutime = IO_rutime / n;
	avg_IO_stime = IO_stime / n;

	printf(1, "Exiting sanity test.\n");
	printf(1,  "CPU Bound process summary:\n\
Average Ready Time:     %d\n\
Average Running time:   %d\n\
Average Sleep Time:     %d\n\
Average Total Run Time: %d\n", avg_cpu_retime, avg_cpu_rutime, avg_cpu_stime, (cpu_retime+cpu_rutime+cpu_stime)/n);
	printf(1,  "Short process summary:\n\
Average Ready Time:     %d\n\
Average Running time:   %d\n\
Average Sleep Time:     %d\n\
Average Total Run Time: %d\n", avg_short_retime, avg_short_rutime, avg_short_stime, (short_retime+short_rutime+short_stime)/n);
	printf(1,  "I/O process summary:\n\
Average Ready Time:     %d\n\
Average Running time:   %d\n\
Average Sleep Time:     %d\n\
Average Total Run Time: %d\n", avg_IO_retime, avg_IO_rutime, avg_IO_stime, (IO_retime+IO_rutime+IO_stime)/n);
	exit();
}

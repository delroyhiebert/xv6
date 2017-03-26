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
	int i, j, k, n, pid;
	n = atoi(argv[1]);//fork 3*n times

	printf(1, "Beginning sanity test.\n");
	for( i = 0; i < 3*n; i++ )
	{
		pid = fork();
		if( pid == 0 )//Child process
		{
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
						yield();
					}
					break;
				case 2:
					for( j = 0; j < 100; j++ )
					{
						sleep(1);
					}
					break;
			}
			
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
	printf(1, "Exiting sanity test.\n");
	printf(1,  "CPU Bound process summary:\n\
Average Ready Time:     %d\n\
Average Running time:   %d\n\
Average Sleep Time:     %d\n\
Average Per Proc Time:  %d\n", cpu_retime, cpu_rutime, cpu_stime, (cpu_retime+cpu_rutime+cpu_stime)/n);
	printf(1,  "Short process summary:\n\
Average Ready Time:     %d\n\
Average Running time:   %d\n\
Average Sleep Time:     %d\n\
Average Per Proc Time:  %d\n", short_retime, short_rutime, short_stime, (short_retime+short_rutime+short_stime)/n);
	printf(1,  "I/O process summary:\n\
Average Ready Time:     %d\n\
Average Running time:   %d\n\
Average Sleep Time:     %d\n\
Average Per Proc Time:  %d\n", IO_retime, IO_rutime, IO_stime, (IO_retime+IO_rutime+IO_stime)/n);
	exit();
}
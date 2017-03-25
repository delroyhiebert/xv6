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
	int i, j, k, n, pid;
	n = atoi(argv[1]);//fork 3*n times

	printf(1, "Beginning sanity test.\n");
	for(i = 0; i < 3*n; i++)
	{
		pid = fork();
		if( pid == 0 )//Child process
		{
			for(j = 0; j < 100; j++)
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
	for (i = 0; i < 3 * n; i++) {
		pid = wait2(&retime, &rutime, &stime);
	}
	printf(1, "Exiting sanity test.\n");
	exit();
}
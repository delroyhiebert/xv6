
#include "types.h"
#include "user.h"

int main (int argc, char* argv[])
{
	if(argc != 2)
	{
		printf(1, "Usage: smlsanity <n>\n");
		exit();
 	}

 	printf(1, "Smlsanity's pid is: %d\n", getpid());

	int n = atoi(argv[1]);
	int i, j, k, pid, retime, rutime, stime;

	printf(1, "Beginning sml test.\n");

	set_prio(3);

	for(i = 0; i < n*5; i++)
	{
		pid = fork();
		if (pid == 0)
		{
			if(set_prio((getpid()%3)+1) == 1)
			{
				printf(1, "set_prio failed.\n");
				exit();
			}
			for( j = 0; j < 500; j++ )
			{
				for( k = 0; k < 1000000; k++ )
				{
					asm("nop");
				}
				yield2();
			}
			printf(1, "Pid: %d, Term time: %d Class: %d\n", getpid(), uptime(), (getpid()%3)+1);
			exit();
		}
	}

	for(i = 0; i < n*5; i++)
	{
		pid = wait2(&retime, &rutime, &stime);
		//printf(1, "Pid: %d, Ready time: %d, Run time: %d, Term time: %d\n", pid, retime, rutime, uptime());
	}

	printf(1, "Terminating.\n");

	exit();
}

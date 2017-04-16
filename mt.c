#include "types.h"
#include "user.h"

#define PAGE_SIZE 4096

int main(int argc, char *argv[])
{
    int* pages[30];
    int i, pid;

    printf(1, "\nStarting myMemTest. Pid is %d.\n", getpid());

	printf(1, "First loop. i is ");
	for(i = 0; i <= 10; i++)
	{
		printf(1, "%d ", i);
        pages[i] = (int*)malloc(PAGE_SIZE);
        memset(pages[i], 'F', PAGE_SIZE);
    }
    printf(1, "\n");

	printf(1, "Second loop. i is ");
    for (i = 10; i >=  0; i--)
	{
		printf(1, "%d ", i);
        pages[i][10] = 10;
        pages[i][100] = 10;
        pages[i][500] = 10;
        pages[i][900] = 10;
    }
    printf(1, "\n");

    if((pid = fork()) == 0)
	{
		//So at this point, we should have inherited our parent's pages.
		//Both in memory, and on swap.

        printf(1, "\nFirst fork. Testing for page inheritance.\n");
		printf(1, "Child pid is %d.\n", getpid());
		printf(1, "Third loop. i is ");
		for (i = 10; i >=  0; i--)
		{
			printf(1, "%d ", i);
            pages[i%4][i*i+3] = 66;
            pages[i][0] = 95;
            pages[i][20] = 10;
            pages[i][21] = 'A';
            pages[i][120] = 10;
            pages[i][121] = 'A';
            pages[i][520] = 10;
            pages[i][521] = 'A';
        }
        printf(1, "\n");
        printf(1, "Child with pid %d exiting.\n\n", getpid());
        exit(); //Child exit
    }
    else
	{
		wait();
        pid = fork();

		if(pid == 0)
        {
			printf(1, "Forks loop. pid is %d.\n", getpid());

            for (i=11; i<17; i++)
			{
                pages[i] = malloc(PAGE_SIZE);
                memset(pages[i], 'M', PAGE_SIZE);
                pages[i%6][1] = 100;
            }
            printf(1, "Forks child with pid %d exiting.\n", getpid());
            exit();
        }
        else
		{
            wait();
        }
    }

    wait();

	printf(1, "\nmyMemTest done, pid is %d.\n", getpid());
	printf(1, "Test successful.\n");

	exit();
}

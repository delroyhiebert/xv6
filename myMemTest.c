#include "types.h"
#include "user.h"

int main (int argc, char* argv[])
{
	if(argc != 2)
	{
		printf(1, "Memtest requires one argument.\n");
		exit();
	}

	int n = atoi(argv[1]);
	int i, j;
	char *heapvar[n];
	char *pointer;

	printf(1, "Interpreting input as: %d\n", n);
	printf(1, "Attempting to allocate %d page(s) of memory.\n", n);

	//Allocate argv[1] pages
	printf(1, "Allocating pages: ");
	for(i = 0; i < n; i++)
	{
		heapvar[i] = (char*)malloc(4096); //Allocate a page
		memset(heapvar[i], (char)i, 4096); //Fill that page with integers.
		printf(1, "%d ", i);
	}
	printf(1, "\n");

	//Test fork inheritance
	if(!fork())
	{
		printf(1, "Child reading pages: ");
		for(i = 0; i < n; i++)
		{
			printf(1, "%d ", i);
			//check each entire page for valid contents
			for(j = 0; j < 4096; j++)
			{
				pointer = heapvar[i];
				pointer = pointer + j;
				if (*pointer != (char)i)
				{
					printf(1, "Heap has been corrupted. Looking for %d, found %d. Terminating.\n", i, *pointer);
					exit();
				}
			}
		}
		printf(1, "\n");
		exit();
	}
	wait();

	//Read the content of those pages back in.
	printf(1, "Parent reading pages: ");
	for(i = 0; i < n; i++)
	{
		printf(1, "%d ", i);
		//check each entire page for valid contents
		for(j = 0; j < 4096; j++)
		{
			pointer = heapvar[i];
			pointer = pointer + j;
			if (*pointer != (char)i)
			{
				printf(1, "Heap has been corrupted. Looking for %d, found %d. Terminating.\n", i, *pointer);
				exit();
			}
		}
	}
	printf(1, "\n");

	printf(1, "Page framework test successful.\n");

	exit();
}
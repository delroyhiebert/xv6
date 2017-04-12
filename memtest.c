
#include "types.h"
#include "user.h"

int main (int argc, char* argv[])
{
	if(argc != 2)
	{
		printf(1, "Memtest requires one argument.\n");
		exit();
	}

	int n = (int)argv[1]; //Yeah, this is unsafe.
	int i, j;
	char *heapvar[n];
	char *pointer;

	printf(1, "Interpreting input as: %d\n", n);
	printf(1, "Attempting to allocate %d pages of memory.\n", n);

	//Allocate argv[1] pages
	for(i = 0; i < n; i++)
	{
		heapvar[i] = (char*)malloc(512); //Allocate a page
		memset(heapvar, (char)i, 512); //Fill that page with integers.
		printf(1, "Allocated page of %d\n", i);
	}

	//Read the content of those pages back in.
	for(i = 0; i < n; i++)
	{
		//check each entire page for valid contents
		for(j = 0; j < 512; j++)
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

	printf(1, "Page framework test successful.\n");

	exit();
}

/*
Written by Delroy Hiebert
Comp 4430 Operating Systems 2
For Dr. Rasit Eskicioglu
*/
#include "signals.h"
#include "types.h"
#include "user.h"

void FPEHandler(void)
{
	printf(1, "Signal handler for SIGFPE...\n");
}

void SEGVHandler(void)
{
	printf(1, "Signal handler for SIGSEGV...\n");
	printf(1, "Killing the process, returning to shell.\n");
	exit();
}

int main(void)
{
	printf(1, "Begin signals test...\n\n");

	printf(1, "Registering SIGFPE handler.\n");
	if( signal(SIGFPE, &FPEHandler) == -1 )
		printf(1, "Failed to set signal handler for FPE signal.\n");
	volatile int aa = 10;
	volatile int bb = 0;
	aa = aa/bb;

	printf(1, "Handler for SIGFPE returned to main\n\n");

	printf(1, "Registering SIGSEGV handler.\n");
	if( signal(SIGSEGV, &SEGVHandler) == -1 )
		printf(1, "Failed to set signal handler for SEGV signal.\n");

	printf(1, "%d\n", *(int*)10000000000);

	printf(1, "Handler for SIGSEGV returned to main, error, should not occur!.\n");

	exit();
}




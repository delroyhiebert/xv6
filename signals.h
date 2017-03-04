/*
Written by Delroy Hiebert
Comp 4430 Operating Systems 2
For Dr. Rasit Eskicioglu
*/
#define SIGKILL -1
#define SIGFPE 	0
#define SIGSEGV	1


typedef void (*sighandler_t)(void);
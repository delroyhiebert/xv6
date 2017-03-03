/*
Written by Delroy Hiebert
Comp 4430 Operating Systems 2
For Dr. Rasit Eskicioglu
*/
#define upArrow 226
#define downArrow 227
#define leftArrow 228
#define rightArrow 229
static int movement;//Cursor movement

#define MAX_HISTORY 16
char history_buffer[MAX_HISTORY][128];
int current_index = 0;
int next_index = 0;
int command_count = 0;

int history(char *buffer, int historyId);











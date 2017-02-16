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

char history_buffer[16][128];
int history_index = 0;
int nextIndex = 0;

int history(char *buffer, int historyId);











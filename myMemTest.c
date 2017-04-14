#include "types.h"
#include "user.h"

#define PAGE_SIZE 4096

int main(int argc, char *argv[])
{
    int* pages[30];
    int i, pid;

    printf(2, "\t starting myMemTest\n");
    for (i=0; i <= 10; i++) {
        printf(2, "\t first loop. i=%d\n", i);
        pages[i] = (int*) malloc(PAGE_SIZE);
        memset(pages[i], 'z', PAGE_SIZE);
    }

    for (i = 10; i >=  0; i--) {
        pages[i][10] = 10;
        pages[i][100] = 10;
        pages[i][500] = 10;
        pages[i][900] = 10;
        printf(2, "\t second loop. i=%d\n", i);
    }

    if ((pid = fork()) == 0) {
        printf(2, "\t child: pid=%d\n", getpid());
        for (i = 10; i >=  0; i--) {
            pages[i%4][i*i+3] = 66;
            pages[i][0] = 95;
            pages[i][20] = 10;
            pages[i][21] = 'A';

            pages[i][120] = 10;
            pages[i][121] = 'A';

            pages[i][520] = 10;
            pages[i][521] = 'A';

            printf(2, "\t third (child) loop. i=%d\n", i);
        }
        printf(2, "\t child exiting\n");
        exit();         /* child exit */
    } else {
        pid = fork();
        if( pid == 0 )
        {
            for (i=11; i<17; i++) {
                printf(2, "forks loop. pid = %d\n", getpid());
                pages[i] = malloc(PAGE_SIZE);
                memset(pages[i], 'M', PAGE_SIZE);
                pages[i%6][1] = 100;
            }
        }
        else {
            wait();
        }
    }
    wait();
    printf(2, "\t myMemTest done pid=%d\n", getpid());
    exit();
}

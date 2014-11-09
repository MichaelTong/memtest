#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define COLUMNS_PER_ROW
#define ROWS_PER_BANK
#define BANKS_PER_RANK 8
#define RANKS_PER_CHANNEL 2
#define NUM_OF_CHANNEL 2


typedef unsigned char BYTE;
typedef unsigned long int UINT32;
typedef unsigned long long int UINT64;


int main(int argc, char* argv[])
{
    struct timespec ts1,ts2,ts3,ts4;
    UINT32 time;

    BYTE *memarray;
    register BYTE reg='1';
    register BYTE *ptr;
    UINT64 totalSize = 64LL<<23;//64MB
    UINT64 totalBytes = totalSize/64;
    UINT64 i;
    memarray = (BYTE *)malloc(totalSize);
    memset(memarray,'1',totalBytes);

    clock_gettime(CLOCK_REALTIME, &ts3);
    for(i = 0;i<totalBytes/64;i++)
    {
        ptr = memarray + i*64*64;

        clock_gettime(CLOCK_REALTIME, &ts1);
        reg = *(ptr);
        clock_gettime(CLOCK_REALTIME, &ts2);
        time = (ts2.tv_sec-ts1.tv_sec)*1000000000 + ts2.tv_nsec-ts1.tv_nsec;

        //printf("%llx\t%c\n",addr,*((BYTE *)addr));
    }
    clock_gettime(CLOCK_REALTIME, &ts4);
    time = (ts4.tv_sec-ts3.tv_sec)*1000000000 + ts4.tv_nsec-ts3.tv_nsec;
    printf("Running time: %ld\n",time);

    free(memarray);
    return 0;
}

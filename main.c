#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "ptlcalls.h"

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
    struct timespec pts_b,pts_e;
    struct timespec wts_b,wts_e;
    struct timespec rts_b,rts_e;
    struct timespec srts_b,srts_e;
    UINT32 time;

    BYTE *memarray;
    register BYTE reg='1';
    register BYTE *ptr;
    UINT64 totalMBytes = 64LL;
    UINT64 totalSize = totalMBytes<<23;//64MB
    UINT64 totalBytes = totalSize/64;
    UINT64 i;

    printf("*********************************************************************\n");
    printf("*                                                                   *\n");
    printf("*          Memory test program, for MARSSx86 with DRAMSim2          *\n");
    printf("*                     Copyright by Michael Tong                     *\n");
    printf("*                                                                   *\n");
    printf("*********************************************************************\n\n");

    //Write
    printf("============Start allocating and setting memory(%lld MB)...============\n\n",totalMBytes);

    memarray = (BYTE *)malloc(totalSize);
    //Starting simulation...
    ptlcall_switch_to_sim();
    clock_gettime(CLOCK_REALTIME, &pts_b);
    clock_gettime(CLOCK_REALTIME, &wts_b);
    memset(memarray,reg,totalBytes);
    clock_gettime(CLOCK_REALTIME, &wts_e);
    time = (wts_e.tv_sec-wts_b.tv_sec)*1000000000 + wts_e.tv_nsec-wts_b.tv_nsec;
    printf("Memory allocation completed.\n");
    printf("Running time: %ld nsec, %lf sec\n\n\n",time,time/1000000000.0);

    //Read
    printf("==================Start testing read performance...==================\n\n");
    clock_gettime(CLOCK_REALTIME, &rts_b);
    for(i = 0;i<totalBytes/64;i++)
    {
        ptr = memarray + i*64;

        //clock_gettime(CLOCK_REALTIME, &srts_b);
        reg = *(ptr);
        //clock_gettime(CLOCK_REALTIME, &srts_e);
        //time = (srts_e.tv_sec-srts_b.tv_sec)*1000000000 + srts_e.tv_nsec-srts_b.tv_nsec;

        //printf("%llx\t%c\n",addr,*((BYTE *)addr));
    }
    clock_gettime(CLOCK_REALTIME, &rts_e);
    clock_gettime(CLOCK_REALTIME, &pts_e);
    ptlcall_switch_to_native();
    time = (rts_e.tv_sec-rts_b.tv_sec)*1000000000 + rts_e.tv_nsec-rts_b.tv_nsec;
    printf("Read performance testing completed.\n");
    printf("Running time: %ld nsec, %lf sec\n\n\n",time,time/1000000000.0);

    free(memarray);
    time = (pts_e.tv_sec-pts_b.tv_sec)*1000000000 + pts_e.tv_nsec-pts_b.tv_nsec;
    printf("===========================Testing Summary===========================\n\n");
    printf("Memory testing completed.\n");
    printf("Running time: %ld nsec, %lf sec\n\n\n",time,time/1000000000.0);

    return 0;
}

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
#define MB_256_IN_BYTES (256LL<<23)/8
#define CB_SIZE 64

typedef unsigned char BYTE;
typedef unsigned long int UINT32;
typedef unsigned long long int UINT64;

void print_time(double *timeRecord, UINT64 len,char *filename)
{
    FILE *fp;
    UINT64 i;
    if((fp=fopen(filename,"w"))==NULL)
    {
        printf("ERROR: Failed to open file %s to write.\n",filename);
        return ;
    }
    fprintf(fp,"Cacheline #\t\t\tAccess Time\n");
    for(i=0;i<len;i++)
    {
        fprintf(fp,"%lld\t\t\t\t%lf\n",i,timeRecord[i]);
    }
    fclose(fp);
    return;
}

int main(int argc, char* argv[])
{
    struct timespec pts_b,pts_e;
    struct timespec wts_b,wts_e;
    struct timespec rts_b,rts_e;
    struct timespec srts_b,srts_e;
    UINT32 time;

    BYTE **memarray;
    register BYTE reg='1';
    register BYTE *ptr;
    register BYTE *base;
    UINT64 totalMBytes = 64LL;
    UINT64 totalSize = totalMBytes<<23;//64MB
    UINT64 totalBytes = totalSize/8;
    UINT64 totalLines = totalBytes/CB_SIZE;
    UINT64 i;
    int j;
    int num_array = totalMBytes/256 + totalMBytes%256==0?0:1;
    double *timeRecord = (double *)calloc(totalLines, sizeof(double));


    printf("*********************************************************************\n");
    printf("*                                                                   *\n");
    printf("*          Memory test program, for MARSSx86 with DRAMSim2          *\n");
    printf("*                     Copyright by Michael Tong                     *\n");
    printf("*                                                                   *\n");
    printf("*********************************************************************\n\n");

    //Write
    printf("============Start allocating and setting memory(%lld MB)...============\n\n",totalMBytes);

    //Starting simulation...
    //ptlcall_switch_to_sim();
    clock_gettime(CLOCK_REALTIME, &pts_b);
    clock_gettime(CLOCK_REALTIME, &wts_b);

    memarray = (BYTE **)malloc(num_array*sizeof(BYTE*));
    for(j=0; j<num_array-1; j++)
    {
        memarray[j]=(BYTE *)malloc(MB_256_IN_BYTES*sizeof(char));
        memset(memarray[j],reg,MB_256_IN_BYTES);
    }
    memarray[j]=(BYTE *)malloc((totalBytes-MB_256_IN_BYTES*j)*sizeof(char));
    memset(memarray[j],reg ,totalBytes-MB_256_IN_BYTES*j);

    clock_gettime(CLOCK_REALTIME, &wts_e);
    time = (wts_e.tv_sec-wts_b.tv_sec)*1000000000 + wts_e.tv_nsec-wts_b.tv_nsec;
    printf("Memory allocation completed.\n");
    printf("Running time: %ld nsec, %lf sec\n\n\n",time,time/1000000000.0);

    //Read
    printf("==================Start testing read performance...==================\n\n");
    base = *(memarray);
    //ptlcall_switch_to_sim();
    clock_gettime(CLOCK_REALTIME, &rts_b);
    for(j=0; j<10; j++)
    {
        for(i = 0; i<totalLines; i++)
        {
            ptr = base + i*CB_SIZE;
            //ptlcall_switch_to_sim();
            clock_gettime(CLOCK_REALTIME, &srts_b);
            reg = *(ptr);
            clock_gettime(CLOCK_REALTIME, &srts_e);
            //ptlcall_switch_to_native();
            time = (srts_e.tv_sec-srts_b.tv_sec)*1000000000 + srts_e.tv_nsec-srts_b.tv_nsec;
            timeRecord[i]+=time;
            //printf("%llx\t%c\n",addr,*((BYTE *)addr));
        }
    }
    clock_gettime(CLOCK_REALTIME, &rts_e);
    clock_gettime(CLOCK_REALTIME, &pts_e);
    //ptlcall_switch_to_native();
    time = (rts_e.tv_sec-rts_b.tv_sec)*1000000000 + rts_e.tv_nsec-rts_b.tv_nsec;
    printf("Read performance testing completed.\n");
    printf("Running time: %ld nsec, %lf sec\n\n\n",time,time/1000000000.0);
    for(i = 0; i<totalLines;i++)
    {
        timeRecord[i]=timeRecord[i]/10;
    }
    print_time(timeRecord,totalLines,"records.txt");
    for(i=0; i<num_array; i++)
        free(memarray[i]);
    free(memarray);
    free(timeRecord);
    time = (pts_e.tv_sec-pts_b.tv_sec)*1000000000 + pts_e.tv_nsec-pts_b.tv_nsec;
    printf("===========================Testing Summary===========================\n\n");
    printf("Memory testing completed.\n");
    printf("Running time: %ld nsec, %lf sec\n\n\n",time,time/1000000000.0);

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include "ptlcalls.h"

#define COLUMNS_PER_ROW
#define ROWS_PER_BANK
#define BANKS_PER_RANK 8
#define RANKS_PER_CHANNEL 2
#define NUM_OF_CHANNEL 2
#define MB_256_IN_BYTES ((256LL<<23)/8)
#define CB_SIZE 64 //in bytes
#define MEASURE_TIME 1

typedef unsigned char BYTE;
typedef unsigned long int UINT32;
typedef unsigned long long int UINT64;

enum
{
    CL_INIT,
    CL_PROGRAM_B,
    CL_PROGRAM_E,
    CL_WRITTING_B,
    CL_WRITTING_E,
    CL_READING_B,
    CL_READING_E,
    CL_SIMULATOR_B,
    CL_SIMULATOR_E,
    CL_STATISTICS_B,
    CL_STATISTICS_E,
    CL_HELP
};

enum
{
    WR_BYTE,
    WR_BLOCK,
    WR_DEFAULT
};

enum
{
    RD_BYTE,
    RD_BLOCK,
    RD_ROW,
    RD_BANK,
    RD_RANK,
    RD_DEFAULT
};

enum
{
    PARAMS_NO,
    PARAMS_M,
    PARAMS_MF,
    PARAMS_R,
    PARAMS_RF,
    PARAMS_W,
    PARAMS_WF,
};

enum
{
    PARAMS_ERR_INVALID,
    PARAMS_ERR_DUPLICATE,
    PARAMS_ERR_LIMIT
};

UINT32 run_time;

BYTE *memarray;
UINT64 totalKBytes;
UINT64 totalMBytes;
UINT64 totalSize;
UINT64 totalBytes;
UINT64 totalLines;
double *timeRecord ;

bool write_sim;
bool write_mode;
bool read_sim;
bool read_mode;

void print_time(double *timeRecord, UINT64 len,char *filename);
void print_info(int cl);
void start_sim();
void stop_sim();
void init(int argc, char* argv[]);
void writing(bool sim, int write_mode);
void reading(bool sim, int read_mode);
void statistics(UINT64 data_size, char *filename);
void cleanup();
void params(int argc, char* argv[]);
void print_params_err(int state, int err);
char* makefilename();
void analyze_mem_layout(UINT64 begin_a, UINT64 size_m, int map_scheme);
void address_mapping(UINT64 physicalAddress, int *chan, int *rank, int *bank, int *row, int *col, int addressMappingScheme);

int main(int argc, char* argv[])
{
    struct timespec pts_b,pts_e;
    init(argc, argv);

    clock_gettime(CLOCK_REALTIME, &pts_b);

    writing(write_sim, write_mode);
    reading(read_sim, read_mode);
    cleanup();

    clock_gettime(CLOCK_REALTIME, &pts_e);
    run_time = (pts_e.tv_sec-pts_b.tv_sec)*1000000000 + pts_e.tv_nsec-pts_b.tv_nsec;
    print_info(CL_PROGRAM_E);

    return 0;
}

void write_record(double *timeRecord, UINT64 len,char *filename)
{
    FILE *fp;
    UINT64 i;
    if((fp=fopen(filename,"w"))==NULL)
    {
        printf("ERROR: Failed to open file %s to write.\n",filename);
        return ;
    }
    fprintf(fp,"Memory Size:\t\t\t%lld MB, %lld KB\n",totalMBytes,totalKBytes);
    fprintf(fp,"Total Reading time:\t\t%ld ns\n\n",run_time);
    fprintf(fp,"Cacheline #\t\t\tAccess Time\n");
    for(i=0; i<len; i++)
    {
        fprintf(fp,"%lld\t\t\t\t%lf\n",i,timeRecord[i]);
    }
    fclose(fp);
    return;
}
void print_info(int cl)
{
    switch (cl)
    {
    case CL_PROGRAM_B:
        //Starting
        printf("*********************************************************************\n");
        printf("*                                                                   *\n");
        printf("*          Memory test program, for MARSSx86 with DRAMSim2          *\n");
        printf("*                     Copyright by Michael Tong                     *\n");
        printf("*                                                                   *\n");
        printf("*********************************************************************\n\n");
        break;
    case CL_WRITTING_B:
        //Allocating and writing
        if(totalMBytes!=0)
            printf("============Start allocating and setting memory(%lld MB)...============\n\n",totalMBytes);
        else
            printf("============Start allocating and setting memory(%lld KB)...============\n\n",totalKBytes);
        break;
    case CL_WRITTING_E:
        //Writing ends
        printf("Memory allocation completed.\n");
        printf("Running time: %ld nsec, %lf sec\n\n\n",run_time,run_time/1000000000.0);
        break;
    case CL_READING_B:
        printf("==================Start testing read performance...==================\n\n");
        break;
    case CL_READING_E:
        printf("Read performance testing completed.\n");
        printf("Running time: %ld nsec, %lf sec\n\n\n",run_time,run_time/1000000000.0);
        break;
    case CL_PROGRAM_E:
        printf("===========================Testing Summary===========================\n\n");
        printf("Memory testing completed.\n");
        printf("Running time: %ld nsec, %lf sec\n\n\n",run_time,run_time/1000000000.0);
        break;
    case CL_INIT:
        printf(">>> Initiate parameters...\n\n");
        break;
    case CL_SIMULATOR_B:
        printf(">>> Simuation starts...\n\n");
        break;
    case CL_SIMULATOR_E:
        printf(">>> Simulation ends...\n\n");
        break;
    case CL_STATISTICS_B:
        printf(">>> Now process reading stastics...\n\n");
        break;
    case CL_STATISTICS_E:
        printf(">>> Finish processing reading stastics...\n\n");
        break;
    case CL_HELP:
        printf("Valid Options:\n");
        printf("-m MEM\t\t\t\t\tAmount of memory for test.(32MB as default)\n");
        printf("-r [BYTE|BLOCK|ROW|BANK|RANK] | [SIM]\tRead interval and Simulator.(BLOCK and false as default)\n");
        printf("-r [BYTE|BLOCK] | [SIM]\t\t\tWrite interval and Simulator.(As a whole and false as default)\n");
    }
}

void writing(bool sim, int write_mode)
{
    struct timespec wts_b,wts_e;
    int j;

    print_info(CL_WRITTING_B);

    if(sim)
    {
        start_sim();
    }
    switch (write_mode)
    {
    case WR_BYTE:
        clock_gettime(CLOCK_REALTIME, &wts_b);
        for(j=0; j<totalBytes; j++)
        {
            memset(memarray+j,'1',1);
        }
        clock_gettime(CLOCK_REALTIME, &wts_e);
        break;

    case WR_BLOCK:
        clock_gettime(CLOCK_REALTIME, &wts_b);
        for(j=0; j<totalLines; j++)
        {
            memset(memarray+64*j,'1',CB_SIZE);
        }
        clock_gettime(CLOCK_REALTIME, &wts_e);
        break;

    case WR_DEFAULT:
    default:
        clock_gettime(CLOCK_REALTIME, &wts_b);
        memset(memarray,'1',totalBytes);
        clock_gettime(CLOCK_REALTIME, &wts_e);
        break;
    }
    if(sim)
    {
        stop_sim();
    }

    run_time = (wts_e.tv_sec-wts_b.tv_sec)*1000000000 + wts_e.tv_nsec-wts_b.tv_nsec;
    print_info(CL_WRITTING_E);

    return;
}

void start_sim()
{
    print_info(CL_SIMULATOR_B);
    ptlcall_switch_to_sim();
}

void stop_sim()
{
    ptlcall_switch_to_native();
    print_info(CL_SIMULATOR_E);
}

void reading(bool sim, int read_mode)
{
    struct timespec rts_b,rts_e;
    //struct timespec srts_b,srts_e;
    register BYTE *ptr;
    register BYTE *base;
    register BYTE reg='1';
    UINT64 read_interval;
    register UINT64 read_size;
   // int j;
    register UINT64 i;

    switch (read_mode)
    {
    case RD_BYTE:
        read_interval = 1;
        read_size = totalBytes;
        break;
    case RD_ROW:
        break;
    case RD_BANK:
        break;
    case RD_RANK:
        break;
    case RD_BLOCK:
    case RD_DEFAULT:
        read_interval = CB_SIZE;
        read_size = totalLines;
        break;
    }

    timeRecord = (double *)calloc(read_size, sizeof(double));
    print_info(CL_READING_B);
    base = memarray;
    if(sim)
        start_sim();
    clock_gettime(CLOCK_REALTIME, &rts_b);
    //for(j=0; j<MEASURE_TIME; j++)
    //{
        for(i = 0; i<read_size; i++)
        {
            ptr = base + i*read_interval;

            //clock_gettime(CLOCK_REALTIME, &srts_b);
            reg = *(ptr);
            //clock_gettime(CLOCK_REALTIME, &srts_e);

            //run_time = (srts_e.tv_sec-srts_b.tv_sec)*1000000000 + srts_e.tv_nsec-srts_b.tv_nsec;
            //timeRecord[i]+=run_time;
        }
    //}
    clock_gettime(CLOCK_REALTIME, &rts_e);
    if(sim)
        stop_sim();
    run_time = (rts_e.tv_sec-rts_b.tv_sec)*1000000000 + rts_e.tv_nsec-rts_b.tv_nsec;
    print_info(CL_READING_E);

    statistics(0/**read_size**/,makefilename());
    return;
}

void statistics(UINT64 data_size, char *filename)
{
    UINT64 i;
    print_info(CL_STATISTICS_B);
    for(i = 0; i<data_size; i++)
    {
        timeRecord[i]=timeRecord[i]/MEASURE_TIME;
    }
    write_record(timeRecord, data_size, filename);
    print_info(CL_STATISTICS_E);
}

void cleanup()
{
    free(memarray);
    free(timeRecord);
}

void params(int argc, char* argv[])
{
    int i;
    int temp;
    int state=PARAMS_NO;
    read_mode=RD_DEFAULT;
    write_mode=WR_DEFAULT;
    totalMBytes = 32;
    read_sim=false;
    write_sim=false;
    bool rm_set=false,rs_set=false,wm_set=false,ws_set=false,m_set=false;
    if(argc==1)
    {
        print_info(CL_HELP);
        exit(-1);
    }
    for(i=1; i<argc; i++)
    {
        if(strcmp(argv[i],"-h")==0)
        {
            print_info(CL_HELP);
            exit(0);
        }
        else if(strcmp(argv[i],"-m")==0)
        {
            if(state!=PARAMS_NO&&state!=PARAMS_MF&&state!=PARAMS_WF&&state!=PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(-1);
            }
            if(m_set)
            {
                print_params_err(PARAMS_M,PARAMS_ERR_DUPLICATE);
                exit(-1);
            }
            state = PARAMS_M;
        }
        else if(strcmp(argv[i],"-r")==0)
        {
            if(state!=PARAMS_NO&&state!=PARAMS_MF&&state!=PARAMS_WF&&state!=PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(-1);
            }
            if(rs_set&&rm_set)
            {
                print_params_err(PARAMS_R,PARAMS_ERR_DUPLICATE);
                exit(-1);
            }
            state = PARAMS_R;
        }
        else if(strcmp(argv[i],"-w")==0)
        {
            if(state!=PARAMS_NO&&state!=PARAMS_MF&&state!=PARAMS_WF&&state!=PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(-1);
            }
            if(ws_set&&wm_set)
            {
                print_params_err(PARAMS_W,PARAMS_ERR_DUPLICATE);
                exit(-1);
            }
            state = PARAMS_W;
        }
        else if(strcmp(argv[i],"SIM")==0)
        {
            if(state != PARAMS_WF && state != PARAMS_W && state != PARAMS_R && state != PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(-1);
            }
            if(state == PARAMS_R|| state == PARAMS_RF)
            {
                if(rs_set)
                {
                    print_params_err(PARAMS_R,PARAMS_ERR_DUPLICATE);
                    exit(-1);
                }
                read_sim=true;
                state = PARAMS_RF;
                rs_set = true;
            }
            else if(state == PARAMS_W||state == PARAMS_WF)
            {
                if(ws_set)
                {
                    print_params_err(PARAMS_W,PARAMS_ERR_DUPLICATE);
                    exit(-1);
                }
                write_sim=true;
                state = PARAMS_WF;
                ws_set = true;
            }
        }
        else if(strcmp(argv[i],"BYTE")==0)
        {
            if(state != PARAMS_WF && state != PARAMS_W && state != PARAMS_R && state != PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(-1);
            }
            if(state == PARAMS_R|| state == PARAMS_RF)
            {
                if(rm_set)
                {
                    print_params_err(PARAMS_R,PARAMS_ERR_DUPLICATE);
                    exit(-1);
                }
                read_mode=RD_BYTE;
                state = PARAMS_RF;
                rm_set = true;
            }
            else if(state == PARAMS_W||state == PARAMS_WF)
            {
                if(wm_set)
                {
                    print_params_err(PARAMS_W,PARAMS_ERR_DUPLICATE);
                    exit(-1);
                }
                write_mode=WR_BYTE;
                state = PARAMS_WF;
                wm_set = true;
            }
        }
        else if(strcmp(argv[i],"BLOCK")==0)
        {
            if(state != PARAMS_WF && state != PARAMS_W && state != PARAMS_R && state != PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(0);
            }
            if(state == PARAMS_R|| state == PARAMS_RF)
            {
                if(rm_set)
                {
                    print_params_err(PARAMS_R,PARAMS_ERR_DUPLICATE);
                    exit(-1);
                }
                read_mode=RD_BLOCK;
                state = PARAMS_RF;
                rm_set = true;
            }
            else if(state == PARAMS_W||state == PARAMS_WF)
            {
                if(wm_set)
                {
                    print_params_err(PARAMS_W,PARAMS_ERR_DUPLICATE);
                    exit(-1);
                }
                write_mode=WR_BLOCK;
                state = PARAMS_WF;
                wm_set = true;
            }
        }
        else if(strcmp(argv[i],"ROW")==0)
        {
            if(state != PARAMS_R && state != PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(0);
            }

            if(rm_set)
            {
                print_params_err(PARAMS_R,PARAMS_ERR_DUPLICATE);
                exit(-1);
            }
            read_mode=RD_ROW;
            state = PARAMS_RF;
            rm_set = true;
        }
        else if(strcmp(argv[i],"BANK")==0)
        {
            if(state != PARAMS_R && state != PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(0);
            }
            if(rm_set)
            {
                print_params_err(PARAMS_R,PARAMS_ERR_DUPLICATE);
                exit(-1);
            }
            read_mode=RD_BANK;
            state = PARAMS_RF;
            rm_set = true;
        }
        else if(strcmp(argv[i],"RANK")==0)
        {

            if(state != PARAMS_R && state != PARAMS_RF)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(0);
            }
            if(rm_set)
            {
                print_params_err(PARAMS_R,PARAMS_ERR_DUPLICATE);
                exit(-1);
            }
            read_mode=RD_RANK;
            state = PARAMS_RF;
            rm_set = true;
        }
        else if((temp = atoi(argv[i])))
        {
            if(state != PARAMS_M)
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(-1);
            }
            if(!isdigit(argv[i][strlen(argv[i])-1])&&argv[i][strlen(argv[i])-1]!='K'&&argv[i][strlen(argv[i])-1]!='M')
            {
                print_params_err(state,PARAMS_ERR_INVALID);
                exit(-1);
            }
            if(argv[i][strlen(argv[i])-1]=='K')
            {
                totalMBytes = 0;
                totalKBytes = temp;
            }
            else if(argv[i][strlen(argv[i])-1]=='M')
            {
                totalMBytes = temp;
            }
            if(totalMBytes<0||temp>=4096)
            {
                print_params_err(state,PARAMS_ERR_LIMIT);
                exit(-1);
            }
            m_set = true;
            state = PARAMS_MF;
        }
        else
        {
            print_params_err(state,PARAMS_ERR_INVALID);
            exit(-1);
        }

    }
    return;
}

void print_params_err(int state, int err)
{
    switch (state)
    {
    case PARAMS_NO:
        if(err == PARAMS_ERR_INVALID)
            fprintf(stderr,"Invalid options.\n");
        break;
    case PARAMS_M:
    case PARAMS_MF:
        if(err == PARAMS_ERR_INVALID)
            fprintf(stderr,"Invalid parameters for option:\t-m\n");
        else if(err == PARAMS_ERR_DUPLICATE)
            fprintf(stderr,"Duplicate parameters for option:\t-m\n");
        else if(err == PARAMS_ERR_LIMIT)
            fprintf(stderr,"Memory limit for -m:\t 0-4096(exclusive)\n");
        break;
    case PARAMS_R:
    case PARAMS_RF:
        if(err == PARAMS_ERR_INVALID)
            fprintf(stderr,"Invalid parameters for option:\t-r\n");
        else if(err == PARAMS_ERR_DUPLICATE)
            fprintf(stderr,"Duplicate parameters for option:\t-r\n");
        break;
    case PARAMS_W:
    case PARAMS_WF:
        if(err == PARAMS_ERR_INVALID)
            fprintf(stderr,"Invalid parameters for option:\t-w\n");
        else if(err == PARAMS_ERR_DUPLICATE)
            fprintf(stderr,"Duplicate parameters for option:\t-w\n");
        break;
    }
    printf("\n");
    print_info(CL_HELP);
}

char* makefilename()
{
    char buff[50],*out;
    time_t t;
    time(&t);
    struct tm *timeinfo = localtime(&t);
    char year[5],month[3],day[3],hour[3],min[3],sec[3];
    sprintf(year,"%d",timeinfo->tm_year+1900);
    if(timeinfo->tm_mon+1<10)
        sprintf(month,"0%d",timeinfo->tm_mon+1);
    else
        sprintf(month,"%d",timeinfo->tm_mon+1);
    if(timeinfo->tm_mday<10)
        sprintf(day,"0%d",timeinfo->tm_mday);
    else
        sprintf(day,"%d",timeinfo->tm_mday);
    if(timeinfo->tm_hour<10)
        sprintf(hour,"0%d",timeinfo->tm_hour);
    else
        sprintf(hour,"%d",timeinfo->tm_hour);
    if(timeinfo->tm_min<10)
        sprintf(min,"0%d",timeinfo->tm_min);
    else
        sprintf(min,"%d",timeinfo->tm_min);
    if(timeinfo->tm_sec<10)
        sprintf(sec,"0%d",timeinfo->tm_sec);
    else
        sprintf(sec,"%d",timeinfo->tm_sec);

    sprintf(buff,"record-%s-%s-%s-%s-%s-%s.txt",year,month,day, hour, min, sec);
    out = (char *)malloc((strlen(buff)+1)*sizeof(char));
    strcpy(out,buff);
    return out;
}
void init(int argc, char* argv[])
{
    params(argc,argv);
    print_info(CL_PROGRAM_B);
    print_info(CL_INIT);

    if(totalMBytes!=0)
    {
        totalSize = totalMBytes<<23;
        totalKBytes = totalMBytes<<10;
    }
    else
        totalSize = totalKBytes<<13;
    totalBytes = totalSize>>3;
    totalLines = totalBytes/CB_SIZE;

    //allocation
    memarray = (BYTE *)malloc(totalBytes*sizeof(BYTE));
    printf("Address of Memarray:\t %llx\n",(UINT64)memarray);
    return;
}
#define NUM_CHANS_LOG 1
#define NUM_RANKS_LOG 1
#define NUM_BANKS_LOG 3
#define NUM_ROWS_LOG 13
#define NUM_COLS_LOG 10
#define BYTE_OFFSET_WIDTH 3
#define TRANSACTION_SIZE 64
#define COL_LOW_BIT_WIDTH 3
unsigned channelBitWidth = NUM_CHANS_LOG;
unsigned rankBitWidth = NUM_RANKS_LOG;
unsigned bankBitWidth = NUM_BANKS_LOG;
unsigned rowBitWidth = NUM_ROWS_LOG;
unsigned colBitWidth = NUM_COLS_LOG;
unsigned colLowBitWidth = COL_LOW_BIT_WIDTH;
unsigned colHighBitWidth;


void analyze_mem_layout(UINT64 begin_a, UINT64 size_m, int map_scheme)
{
	UINT64 tempA, tempB, end_a;
	unsigned transactionSize = TRANSACTION_SIZE;
	UINT64 transactionMask =  transactionSize - 1; //ex: (64 bit bus width) x (8 Burst Length) - 1 = 64 bytes - 1 = 63 = 0x3f mask
	// this forces the alignment to the width of a single burst (64 bits = 8 bytes = 3 address bits for DDR parts)
	unsigned byteOffsetWidth = BYTE_OFFSET_WIDTH;
	// Since we're assuming that a request is for BL*BUS_WIDTH, the bottom bits
	// of this address *should* be all zeros if it's not, issue a warning
    colHighBitWidth = colBitWidth - colLowBitWidth;
	if ((begin_a & transactionMask) != 0)
	{
		printf("WARNING: address 0x%llx is not aligned to the request size of %d.\n", begin_a,transactionSize);
	}

	// each burst will contain JEDEC_DATA_BUS_BITS/8 bytes of data, so the bottom bits (3 bits for a single channel DDR system) are
	// 	thrown away before mapping the other bits
	end_a = begin_a + size_m -1;
	begin_a >>= byteOffsetWidth;
	end_a >>= byteOffsetWidth;


	// The next thing we have to consider is that when a request is made for a
	// we've taken into account the granulaity of a single burst by shifting
	// off the bottom 3 bits, but a transaction has to take into account the
	// burst length (i.e. the requests will be aligned to cache line sizes which
	// should be equal to transactionSize above).
	//
	// Since the column address increments internally on bursts, the bottom n
	// bits of the column (colLow) have to be zero in order to account for the
	// total size of the transaction. These n bits should be shifted off the
	// address and also subtracted from the total column width.
	//
	// I am having a hard time explaining the reasoning here, but it comes down
	// this: for a 64 byte transaction, the bottom 6 bits of the address must be
	// zero. These zero bits must be made up of the byte offset (3 bits) and also
	// from the bottom bits of the column
	//
	// For example: cowLowBits = log2(64bytes) - 3 bits = 3 bits

	begin_a >>= colLowBitWidth;
	end_a >>= colLowBitWidth;
	printf("Bit widths: chan:%d rank:%d bank:%d row:%d colLow:%d colHigh:%d off:%d Total:%d\n\n",channelBitWidth,rankBitWidth,bankBitWidth,rowBitWidth,colLowBitWidth
				,colHighBitWidth,byteOffsetWidth,(channelBitWidth + rankBitWidth + bankBitWidth + rowBitWidth + colLowBitWidth + colHighBitWidth + byteOffsetWidth));


}

void address_mapping(UINT64 physicalAddress, int *chan, int *rank, int *bank, int *row, int *col, int addressMappingScheme)
{
	UINT64 tempA, tempB;
	//perform various address mapping schemes
	if (addressMappingScheme == 1)
	{
		//chan:rank:row:col:bank
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		*bank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colHighBitWidth;
		tempB = physicalAddress << colHighBitWidth;
		*col = tempA ^ tempB;
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		*row = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		*rank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> channelBitWidth;
		tempB = physicalAddress << channelBitWidth;
		*chan = tempA ^ tempB;

	}
	else if (addressMappingScheme == 2)
	{
		//*chan:*row:*col:*bank:*rank
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		*rank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		*bank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colHighBitWidth;
		tempB = physicalAddress << colHighBitWidth;
		*col = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		*row = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> channelBitWidth;
		tempB = physicalAddress << channelBitWidth;
		*chan = tempA ^ tempB;

	}
	else if (addressMappingScheme == 3)
	{
		//*chan:*rank:*bank:*col:*row
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		*row = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colHighBitWidth;
		tempB = physicalAddress << colHighBitWidth;
		*col = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		*bank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		*rank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> channelBitWidth;
		tempB = physicalAddress << channelBitWidth;
		*chan = tempA ^ tempB;

	}
	else if (addressMappingScheme == 4)
	{
		//*chan:*rank:*bank:*row:*col
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colHighBitWidth;
		tempB = physicalAddress << colHighBitWidth;
		*col = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		*row = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		*bank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		*rank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> channelBitWidth;
		tempB = physicalAddress << channelBitWidth;
		*chan = tempA ^ tempB;

	}
	else if (addressMappingScheme == 5)
	{
		//*chan:*row:*col:*rank:*bank

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		*bank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		*rank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colHighBitWidth;
		tempB = physicalAddress << colHighBitWidth;
		*col = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		*row = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> channelBitWidth;
		tempB = physicalAddress << channelBitWidth;
		*chan = tempA ^ tempB;


	}
	else if (addressMappingScheme == 6)
	{
		//*chan:*row:*bank:*rank:*col

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colHighBitWidth;
		tempB = physicalAddress << colHighBitWidth;
		*col = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		*rank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		*bank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		*row = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> channelBitWidth;
		tempB = physicalAddress << channelBitWidth;
		*chan = tempA ^ tempB;


	}
	// clone of scheme 5, but channel moved to lower bits
	else if (addressMappingScheme == 7)
	{
		//*row:*col:*rank:*bank:*chan
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> channelBitWidth;
		tempB = physicalAddress << channelBitWidth;
		*chan = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		*bank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		*rank = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colHighBitWidth;
		tempB = physicalAddress << colHighBitWidth;
		*col = tempA ^ tempB;

		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		*row = tempA ^ tempB;

	}
}

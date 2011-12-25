#ifndef UDPCAST_H
#define UDPCAST_H

#include "socklib.h"
#include <sys/time.h>

#define BITS_PER_INT (sizeof(int) * 8)
#define BITS_PER_CHAR 8


#define MAP_ZERO(l, map) (memset(map, 0, ((l) + BITS_PER_INT - 1)/ BIT_PER_INT))
#define BZERO(data) (memset((void *)&data, 0, sizeof(data)))


#define RDATABUFSIZE (2*(MAX_SLICE_SIZE + 1)* MAX_BLOCK_SIZE)

#define DATABUFSIZE (RDATABUFSIZE + 4096 - RDATABUFSIZE % 4096)

#define writeSize udpc_writeSize
#define largeReadSize udpc_largeReadSize
#define smallReadSize udpc_smallReadSize
#define makeDataBuffer udpc_makeDataBuffer
#define parseCommand udpc_parseCommand
#define printLongNum udpc_printLongNum
#define waitForProcess udpc_waitForProcess

int writeSize(void);
int largeReadSize(void);
int smallReadSize(void);
int makeDataBuffer(int blocksize);
int parseCommand(char *pipeName, char **arg);

int printLongNum(unsigned long long x);
int waitForProcess(int pid, char *message);

struct disk_config {
    int origOutFile;
    char *fileName;
    char *pipeName;
    int flags;
};

struct net_config {
    net_if_t *net_if; /* Network interface (eth0, isdn0, etc.) on which to
                       * multicast */
    int portBase; /* Port base */
    int blockSize;
    int sliceSize;
    struct sockaddr_in controlMcastAddr;
    struct sockaddr_in dataMcastAddr;
    char *mcastRdv;
    int ttl;
    struct rate_limit *rateLimit;
    /*int async;*/
    /*int pointopoint;*/
    int dir; /* 1 if TIOCOUTQ is remaining space, 
              * 0 if TIOCOUTQ is consumed space */
    int sendbuf; /* sendbuf */
    struct timeval ref_tv;

    enum discovery {
        DSC_DOUBLING,
        DSC_REDUCING
    } discovery;

    /* int autoRate; do queue watching using TIOCOUTQ, to avoid overruns */

    int flags; /* non-capability command line flags */
    int capabilities;

    int min_slice_size;
    int default_slice_size;
    int max_slice_size;
    unsigned int rcvbuf;

    int rexmit_hello_interval; /* retransmission interval between hello's.
                                * If 0, hello message won't be retransmitted
                                */
    int autostart; /* autostart after that many retransmits */

    int requestedBufSize; /* requested receiver buffer */

    /* sender-specific parameters */
    int min_receivers;
    int max_receivers_wait;
    int min_receivers_wait;

    int retriesUntilDrop;

    /* receiver-specif parameters */
    int exitWait; /* How many milliseconds to wait on program exit */

    int startTimeout; /* Timeout at start */

    /* FEC config */
#ifdef BB_FEATURE_UDPCAST_FEC
    int fec_redundancy; /* how much fec blocks are added per group */
    int fec_stripesize; /* size of FEC group */
    int fec_stripes; /* number of FEC stripes per slice */
#endif
};

struct stat_config {
    FILE *log; /* Log file for statistics */
    long bwPeriod; /* How often are bandwidth estimations logged? */
};

#define MAX_SLICE_SIZE 1024

#ifndef DEBUG
# define DEBUG 0
#endif

#endif

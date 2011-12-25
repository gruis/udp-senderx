#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifndef UDPCAST_CONFIG_H
# define UDPCAST_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_LOFF_T
typedef unsigned long long loff_t;
#endif

#include <sys/time.h>
#include <unistd.h>
#include "log.h"
#include "statistics.h"
#include "util.h"
#include "socklib.h"

struct receiver_stats {
    struct timeval tv_start;
    int bytesOrig;
    long long totalBytes;
    int timerStarted;
    int fd;
};

receiver_stats_t allocReadStats(int fd) {
    receiver_stats_t rs =  MALLOC(struct receiver_stats);
    rs->fd = fd;
    return rs;
}

void receiverStatsAddBytes(receiver_stats_t rs, long bytes) {
    if(rs != NULL)
	rs->totalBytes += bytes;
}

void receiverStatsStartTimer(receiver_stats_t rs) {
    if(rs != NULL && !rs->timerStarted) {
	gettimeofday(&rs->tv_start, 0);
	rs->timerStarted = 1;
    }
}

static void printFilePosition(int fd) {
    if(fd != -1) {
#ifdef HAVE_LSEEK64
	loff_t offset = lseek64(fd, 0, SEEK_CUR);
	printLongNum(offset);
#else
	off_t offset = lseek(fd, 0, SEEK_CUR);
	fprintf(stderr, "%10d", offset);
#endif
    }
}

void displayReceiverStats(receiver_stats_t rs) {
    long long timePassed;
    struct timeval tv_now;

    if(rs == NULL)
	return;

    gettimeofday(&tv_now, 0);

    fprintf(stderr, "bytes=");
    printLongNum(rs->totalBytes);
    fprintf(stderr, " (");
    
    timePassed = tv_now.tv_sec - rs->tv_start.tv_sec;
    timePassed *= 1000000;
    timePassed += tv_now.tv_usec - rs->tv_start.tv_usec;
    if (timePassed != 0) {
	int mbps = (int) (rs->totalBytes * 800 / timePassed);
	fprintf(stderr, "%3d.%02d", mbps / 100, mbps % 100);
    } else {
	fprintf(stderr, "***.**");
    }
    fprintf(stderr, " Mbps)");
    printFilePosition(rs->fd);
    fprintf(stderr, "\r");
    fflush(stderr);
}


struct sender_stats {
    FILE *log;    
    unsigned long long totalBytes;
    unsigned long long retransmissions;
    int fd;
    int clNo;
    unsigned long periodBytes;
    struct timeval periodStart;
    long bwPeriod;
};

sender_stats_t allocSenderStats(int fd, FILE *logfile, long bwPeriod) {
    sender_stats_t ss = MALLOC(struct sender_stats);
    ss->fd = fd;
    ss->log = logfile;
    ss->bwPeriod = bwPeriod;
    gettimeofday(&ss->periodStart, 0);
    return ss;
}

void senderStatsAddBytes(sender_stats_t ss, long bytes) {
    if(ss != NULL) {
	ss->totalBytes += bytes;

	if(ss->bwPeriod) {
	    double tdiff, bw;
	    struct timeval tv;
	    gettimeofday(&tv, 0);
	    ss->periodBytes += bytes;
	    if(tv.tv_sec - ss->periodStart.tv_sec < ss->bwPeriod-1)
		return;
	    tdiff = (tv.tv_sec-ss->periodStart.tv_sec) * 1000000.0 +
		tv.tv_usec - ss->periodStart.tv_usec;
	    if(tdiff < ss->bwPeriod * 1000000.0)
		return;
	    bw = ss->periodBytes * 8.0 / tdiff;
	    ss->periodBytes=0;
	    ss->periodStart = tv;
	    logprintf(ss->log, "Inst BW=%f\n", bw);
	    fflush(ss->log);
	}
    }
}

void senderStatsAddRetransmissions(sender_stats_t ss, int retransmissions) {
    if(ss != NULL) {
	ss->retransmissions += retransmissions;
	logprintf(ss->log, "RETX %9lld %4d\n", ss->retransmissions, 
		  retransmissions);
    }
}


void displaySenderStats(sender_stats_t ss, int blockSize, int sliceSize) {
    unsigned int blocks, percent;
    
    if(ss == NULL)
	return;
    blocks = (ss->totalBytes + blockSize - 1) / blockSize;
    percent = (1000L * ss->retransmissions) / blocks;
    
    fprintf(stderr, "bytes=");
    printLongNum(ss->totalBytes);
    fprintf(stderr, "re-xmits=%07llu (%3u.%01u%%) slice=%04d ",
	    ss->retransmissions, percent / 10, percent % 10, sliceSize);
    printFilePosition(ss->fd);
    fprintf(stderr, "- %3d\r", ss->clNo);
    fflush(stderr);
}

void senderSetAnswered(sender_stats_t ss, int clNo) {
    if(ss != NULL)
	ss->clNo = clNo;
}

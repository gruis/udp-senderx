#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "socklib.h"
#include "rate-limit.h"
#include "util.h"

struct rate_limit {
    long long date;
    long long realDate;
    int bitrate;
    int queueSize;
};

#define MILLION 1000000
#define LLMILLION ((long long)1000000)

static long long getLongLongDate(void) {
    long long date;
    struct timeval tv;
    gettimeofday(&tv,0);
    date = (long long) tv.tv_sec;
    date *= LLMILLION;
    date += (long long) tv.tv_usec;
    return date;
}

struct rate_limit *allocRateLimit(int bitrate) {
    struct rate_limit *rateLimit = MALLOC(struct rate_limit);
    rateLimit->date = getLongLongDate();
    rateLimit->bitrate = bitrate;
    rateLimit->queueSize = 0;
    return rateLimit;
}

int doRateLimit(struct rate_limit *rateLimit, int size) {
    if(rateLimit) {
	long long now = getLongLongDate();
	long long elapsed = now - rateLimit->date;
	long long bits = elapsed * ((long long)rateLimit->bitrate) / LLMILLION;
	int sleepTime;
	size += 28; /* IP header size */

	if(bits >= rateLimit->queueSize * 8) {
	    rateLimit->queueSize = size;
	    rateLimit->date = now;
	    return 0;
	}
	
	rateLimit->queueSize -= bits / 8;
	rateLimit->date += bits * LLMILLION / rateLimit->bitrate;
	rateLimit->realDate = now;
	sleepTime = rateLimit->queueSize * 8 * LLMILLION / rateLimit->bitrate;
	if(sleepTime > 40000 || rateLimit->queueSize >= 100000) {
	    sleepTime -= 10000;
	    sleepTime -= sleepTime % 10000;
	    usleep(sleepTime);
	}
	/*flprintf(".");*/
	rateLimit->queueSize += size;
    }
    return 0;
}


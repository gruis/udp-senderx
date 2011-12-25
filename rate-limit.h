#ifndef RATE_LIMIT_H
#define RATE_LIMIT_H

#include "libbb_udpcast.h"

#define doRateLimit udpc_doRateLimit
#define allocRateLimit udpc_allocRateLimit

struct rate_limit;
int doRateLimit(struct rate_limit *rateLimit, int size);
struct rate_limit *allocRateLimit(int bitrate);

#endif

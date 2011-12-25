#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include "log.h"

static int needNewline=0;

static void printNewlineIfNeeded(void) {
    if (needNewline) {
	fprintf(stderr, "\n");
    }
    needNewline=0;
}


/**
 * Print message to the log, if not null
 */
int logprintf(FILE *logfile, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if(logfile != NULL) {	
	char buf[9];
	struct timeval tv;
	int r;
	gettimeofday(&tv, NULL);
	strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&tv.tv_sec));
	fprintf(logfile, "%s.%06ld ", buf, tv.tv_usec);
	r= vfprintf(logfile, fmt, ap);
	fflush(logfile);
	return r;
    } else
	return -1;
}

/**
 * Print message to stdout, adding a newline "if needed"
 * A newline is needed if this function has not yet been invoked
 * since last statistics printout
 */
int flprintf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);

    printNewlineIfNeeded();
    return vfprintf(stderr, fmt, ap);
}

volatile int quitting = 0;

/**
 * Print message to stdout, adding a newline "if needed"
 * A newline is needed if this function has not yet been invoked
 * since last statistics printout
 */
int fatal(int code, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);

    if(quitting)
	_exit(code);
    quitting=1;
    
    printNewlineIfNeeded();
    vfprintf(stderr, fmt, ap);
#if 0
    assert(0); /* make it easyer to use a debugger to see where this came 
		* from */
#endif
    exit(code);
}

int printLongNum(unsigned long long x) {
/*    fprintf(stderr, "%03d ", (int) ( x / 1000000000000LL   ));*/
    long long divisor;
    int nonzero;

    divisor = 1000000000LL;
    nonzero = 0;

    while(divisor) {
	int digits;
	char *format;

	digits = (int) ((x / divisor) % 1000);
	if (nonzero) {
	    format = "%03d ";
	} else {
	    format = "%3d ";
	}
	if (digits || nonzero)
	    fprintf(stderr, format, digits);
	else
	    fprintf(stderr, "    ");
	    
	if(digits) {
	    nonzero = 1;
	}
	divisor = divisor / 1000;
    }
    needNewline = 1;
    return 0;
}

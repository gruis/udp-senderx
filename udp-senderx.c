#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include "log.h"
#include "socklib.h"
#include "udpcast.h"
#include "udp-sender.h"
#include "rate-limit.h"
#include "udpc_version.h"

#ifdef USE_SYSLOG
#include <syslog.h>
#endif

#ifdef BB_FEATURE_UDPCAST_FEC
#include "fec.h"
#endif

static struct option options[] = {
    { "file", 1, NULL, 'f' },
    { "half-duplex", 0, NULL, 'c' },
    { "full-duplex", 0, NULL, 'd' },
    { "pipe", 1, NULL, 'p' },
    { "port", 1, NULL, 'P' },
    { "portbase", 1, NULL, 'P' },
    { "blocksize", 1, NULL, 'b' },
    { "interface", 1, NULL, 'i' },
    { "mcast_address", 1, NULL, 'm' }, /* Obsolete name */
    { "mcast-address", 1, NULL, 'm' }, /* Obsolete name */
    { "mcast_data_address", 1, NULL, 'm' },
    { "mcast-data-address", 1, NULL, 'm' },
    { "mcast_all_address", 1, NULL, 'M' }, /* Obsolete name */
    { "mcast-all-address", 1, NULL, 'M' }, /* Obsolete name */
    { "mcast_rdv_address", 1, NULL, 'M' },
    { "mcast-rdv-address", 1, NULL, 'M' },
    { "max_bitrate", 1, NULL, 'r' },
    { "max-bitrate", 1, NULL, 'r' },
    { "point-to-point", 0, NULL, '1' },
    { "point_to_point", 0, NULL, '1' },
    { "pointopoint", 0, NULL, '1' },

    { "nopoint-to-point", 0, NULL, '2' },
    { "nopoint_to_point", 0, NULL, '2' },
    { "nopointopoint", 0, NULL, '2' },

    { "async", 0, NULL, 'a' },
    { "autorate", 0, NULL, 'A' },
    { "log", 1, NULL, 'l' },

    /* slice size configuration */
    { "min-slice-size", 1, NULL, 0x0101 },
    { "default-slice-size", 1, NULL, 0x0102 },
    { "slice-size", 1, NULL, 0x0102 },
    { "max-slice-size", 1, NULL, 0x0103 },

    { "ttl", 1, NULL, 0x0401 },
#ifdef BB_FEATURE_UDPCAST_FEC
    { "fec", 1, NULL, 0x0502 },
    { "license", 0, NULL, 'L' },
#endif

#ifdef LOSSTEST
    /* simulating packet loss */
    { "write-loss", 1, NULL, 0x601 },
    { "read-loss", 1, NULL, 0x602 },
    { "seed", 1, NULL, 0x603 },
    { "print-seed", 0, NULL, 0x604 },
#endif

    { "rexmit-hello-interval", 1, NULL, 0x701 },
    { "autostart", 1, NULL, 0x702 },

    { "broadcast", 0, NULL, 0x801 },

    { "sendbuf", 1, NULL, 0x0a01 },

    { "min-clients", 1, NULL, 0xb01 }, /* Obsolete name */
    { "min-receivers", 1, NULL, 0xb01 },
    { "max-wait", 1, NULL, 0xb02 },
    { "min-wait", 1, NULL, 0xb03 },
    { "nokbd", 0, NULL, 0xb04 },

    { "retriesUntilDrop", 1, NULL, 0xc01 }, /* Obsolete name */
    { "retries-until-drop", 1, NULL, 0xc01 },

    { "daemon-mode", 0, NULL, 0xd01},

    { "bw-period", 1, NULL, 0xe01},
    { NULL, 0, NULL, 0}
};

#ifdef NO_BB
static void usage(char *progname) {
    fprintf(stderr, "%s [--file file] [--full-duplex] [--pipe pipe] [--portbase portbase] [--blocksize size] [--interface net-interface] [--mcast-data-address data-mcast-address] [--mcast-rdv-address mcast-rdv-address] [--max-bitrate bitrate] [--pointopoint] [--async] [--log file] [--min-slice-size min] [--max-slice-size max] [--slice-size] [--ttl time-to-live] [--fec <stripes>x<redundancy>/<stripesize>] [--print-seed] [--rexmit-hello-interval interval] [--autostart autostart] [--broadcast] [--min-receivers receivers] [--min-wait sec] [--max-wait sec] [--retries-until-drop n] [--nokbd] [--bw-period n] [--license]\n", progname); /* FIXME: copy new options to busybox */
    exit(1);
}
#else
static inline void usage(char *progname) {
    bb_show_usage();
}
#endif

#ifndef NO_BB
int udpsender_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    int c;
    char *ptr;
#ifdef LOSSTEST
    int seedSet = 0;
    int printSeed = 0;
#endif

    int daemon_mode = 0;
    int r;
    struct net_config net_config;
    struct disk_config disk_config;
    struct stat_config stat_config;
    char *ifName = NULL;

    /* argument parsing */
    disk_config.fileName = NULL;
    disk_config.pipeName = NULL;
    disk_config.flags = 0;

    clearIp(&net_config.dataMcastAddr);
    net_config.mcastRdv = NULL;
    net_config.blockSize = 1456;
    net_config.sliceSize = 16;
    net_config.portBase = 9000;
    net_config.rateLimit = NULL;
    net_config.flags = 0;
    net_config.capabilities = 0;
    net_config.min_slice_size = 16;
    net_config.max_slice_size = 1024;
    net_config.default_slice_size = 0;
    net_config.ttl = 1;
    net_config.rexmit_hello_interval = 0;
    net_config.autostart = 0;
    net_config.requestedBufSize = 0;

    net_config.min_receivers=0;
    net_config.max_receivers_wait=0;
    net_config.min_receivers_wait=0;

    net_config.retriesUntilDrop = 200;

    stat_config.log = NULL;
    stat_config.bwPeriod = 0;

    ptr = strrchr(argv[0], '/');
    if(!ptr)
        ptr = argv[0];
    else
        ptr++;

    net_config.net_if = NULL;
    if (strcmp(ptr, "init") == 0) {
        disk_config.pipeName = strdup("/bin/gzip -c");
        disk_config.fileName = "/dev/hda";
    } else {
        while( (c=getopt_long(argc, argv, 
                              "f:p:P:b:i:m:l:r:a1AcdM:L",
                              options, NULL)) != EOF ) {
            switch(c) {
                case 'a':
                    net_config.flags |= FLAG_ASYNC|FLAG_SN;
                    break;
                case 'c':
                    net_config.flags &= ~FLAG_SN;
                    net_config.flags |= FLAG_NOTSN;
                    break;
                case 'd':
                    net_config.flags |= FLAG_SN;
                    break;                  
                case 'f':
                    disk_config.fileName=optarg;
                    break;
                case 'i':
                    ifName=optarg;
                    break;
                case 'p':
                    disk_config.pipeName=optarg;
                    break;
                case 'P':
                    net_config.portBase = atoi(optarg);
                    break;
                case '1':
                    net_config.flags |= FLAG_POINTOPOINT;
                    break;
                case '2':
                    net_config.flags |= FLAG_NOPOINTOPOINT;
                    break;
                case 'b':
                    net_config.blockSize = strtoul(optarg, 0, 0);
                    net_config.blockSize -= net_config.blockSize % 4;
                    if (net_config.blockSize <= 0) {
                        perror("block size too small");
                        exit(1);
                    }
#if 0
                    if (net_config.blockSize > 1456) {
                        perror("block size too large");
                        exit(1);
                    }
#endif
                    break;
                case 'l':
                    stat_config.log = udpc_log = fopen(optarg, "a");
                    break;
                case 'm':
                    setIpFromString(&net_config.dataMcastAddr, optarg);
                    ipIsZero(&net_config.dataMcastAddr);
                    break;
                case 'M':
                    net_config.mcastRdv = strdup(optarg);
                    break;
                case 'r':
                    net_config.rateLimit=allocRateLimit(parseSpeed(optarg));
                    break;
                case 'A':
#ifdef FLAG_AUTORATE
                    net_config.flags |= FLAG_AUTORATE;
#else
                    fatal(1, 
                          "Auto rate limit not supported on this platform\n");
#endif
                    break;
                case 0x0101:
                    net_config.min_slice_size = atoi(optarg);
                    if(net_config.min_slice_size > MAX_SLICE_SIZE)
                        fatal(1, "min slice size too big\n");
                    break;
                case 0x0102:
                    net_config.default_slice_size = atoi(optarg);
                    break;
                case 0x0103:
                    net_config.max_slice_size = atoi(optarg);
                    if(net_config.max_slice_size > MAX_SLICE_SIZE)
                        fatal(1, "max slice size too big\n");
                    break;
                case 0x0401:
                    net_config.ttl = atoi(optarg);
                    break;
#ifdef BB_FEATURE_UDPCAST_FEC
                case 0x502:
                    net_config.flags |= FLAG_FEC;
                    {
                        char *eptr;
                        ptr = strchr(optarg, 'x');
                        if(ptr) {
                            net_config.fec_stripes = 
                                strtoul(optarg, &eptr, 10);
                            if(ptr != eptr) {
                                flprintf("%s != %s\n", ptr, eptr);
                                usage(argv[0]);
                            }
                            ptr++;
                        } else {
                            net_config.fec_stripes = 8;
                            ptr = optarg;
                        }
                        net_config.fec_redundancy = strtoul(ptr, &eptr, 10);
                        if(*eptr == '/') {
                            ptr = eptr+1;
                            net_config.fec_stripesize = 
                                strtoul(ptr, &eptr, 10);
                        } else {
                            net_config.fec_stripesize = 128;
                        }
                        if(*eptr) {
                            flprintf("string not at end %s\n", eptr);
                            usage(argv[0]);
                        }
                        fprintf(stderr, "stripes=%d redund=%d stripesize=%d\n",
                                net_config.fec_stripes,
                                net_config.fec_redundancy,
                                net_config.fec_stripesize);
                    }
                    break;
            case 'L':
                    fec_license();
                    break;
#endif
#ifdef LOSSTEST
                case 0x601:
                    setWriteLoss(optarg);
                    break;
                case 0x602:
                    setReadLoss(optarg);
                    break;
                case 0x603:
                    seedSet=1;
                    srandom(strtoul(optarg,0,0));
                    break;
                case 0x604:
                    printSeed=1;
                    break;
#endif
                case 0x701:
                    net_config.rexmit_hello_interval = atoi(optarg);
                    break;
                case 0x702:
                    net_config.autostart = atoi(optarg);
                    break;
                case 0x801:
                    net_config.flags |= FLAG_BCAST;
                    break;                  
                case 0xa01:
                    net_config.requestedBufSize=parseSize(optarg);
                    break;                  
                case 0xb01:
                    net_config.min_receivers = atoi(optarg);
                    break;
                case 0xb02:
                    net_config.max_receivers_wait = atoi(optarg);
                    break;
                case 0xb03:
                    net_config.min_receivers_wait = atoi(optarg);
                    break;
                case 0xb04:
                    net_config.flags |= FLAG_NOKBD;
                    break;

                case 0xc01:
                    net_config.retriesUntilDrop = atoi(optarg);
                    break;

                case 0xd01:
                    daemon_mode = 1;
                    break;

                case 0xe01:
                    stat_config.bwPeriod = atol(optarg);
                    break;
                case '?':
                    usage(argv[0]);
            }
        }
    }

    if(optind < argc && !disk_config.fileName) {
        disk_config.fileName = argv[optind++];
    }

    if(optind < argc) {
        fprintf(stderr, "Extra argument \"%s\" ignored\n", argv[optind]);
    }

    if((net_config.flags & FLAG_POINTOPOINT) &&
       (net_config.flags & FLAG_NOPOINTOPOINT)) {
        fatal(1,"pointopoint and nopointopoint cannot be set both\n");
    }

    if( (net_config.autostart || (net_config.flags & FLAG_ASYNC)) &&
        net_config.rexmit_hello_interval == 0)
        net_config.rexmit_hello_interval = 1000;

#ifdef LOSSTEST
    if(!seedSet)
        srandomTime(printSeed);
#endif
    if(net_config.flags &  FLAG_ASYNC) {
        if(net_config.rateLimit == 0) {
            fprintf(stderr, 
                    "Async mode chosen but no rate-limit ==> unsafe\n");
            fprintf(stderr, 
                    "Transmission would fail due to buffer overrung\n");
            fprintf(stderr, 
                    "Add \"--max-bitrate 9500k\" to commandline (for example)\n");
            exit(1);
        }
#ifdef BB_FEATURE_UDPCAST_FEC
        if(! (net_config.flags & FLAG_FEC)) 
#endif
          {
            fprintf(stderr, 
                    "Warning: Async mode but no forward error correction\n");
            fprintf(stderr, 
                    "Transmission may fail due to packet loss\n");
            fprintf(stderr, 
                    "Add \"--fec 8x8\" to commandline\n");
        }
    }

    if(net_config.min_slice_size < 1)
        net_config.min_slice_size = 1;
    if(net_config.max_slice_size < net_config.min_slice_size)
        net_config.max_slice_size = net_config.min_slice_size;
    if(net_config.default_slice_size != 0) {
        if(net_config.default_slice_size < net_config.min_slice_size)
            net_config.default_slice_size = net_config.min_slice_size;
        if(net_config.default_slice_size > net_config.max_slice_size)
            net_config.default_slice_size = net_config.max_slice_size;
    }

    fprintf(stderr, "Udp-senderx %s\n", version);

    /*
    if(disk_config.fileName == NULL && disk_config.pipeName == NULL) {
        fatal(1, "You must supply file or pipe\n");
    }
     end of argument parsing */

#ifdef USE_SYSLOG
    openlog((const char *)"udpcast", LOG_NDELAY|LOG_PID, LOG_SYSLOG);
#endif
    
    do {
        r= startSender(&disk_config, &net_config, &stat_config, ifName);
    } while(daemon_mode);
    return r;
}

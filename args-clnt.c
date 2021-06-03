#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <getopt.h>

#include "common.h"
#include "log.h"
#include "args.h"

const char short_options[] = "d:?iP:w:h:f:D:b";

const struct option
        long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "help",   no_argument,       NULL, '?' },
        { "info",   no_argument,       NULL, 'i' },
        { "port",   required_argument, NULL, 'P' },
        { "width",  required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "frate",  required_argument, NULL, 'f' },
        { "count",  required_argument, NULL, 'c' },
        { "debug",  required_argument, NULL, 'D' },
        { "background",  required_argument, NULL, 'b' },
        { 0, 0, 0, 0 }
};


static void set_defaults(struct Args_inst *argsInst )
{
    log_set_level(LOG_LEVEL_DEFAULT);

    strcpy(argsInst->v4l2loopback_dev, VIDEO_DEV_DEFAULT);
    argsInst->width = FRAME_W_DEFAULT;
    argsInst->height = FRAME_H_DEFAULT;
    argsInst->frame_rate = FRAME_RATE_DEFAULT;

    strcpy(argsInst->ip_addr, IP_ADDR_DEFAULT);
    argsInst->ip_port = IP_PORT_DEFAULT;
    argsInst->ip_unix = NULL;
    argsInst->run_mode = FOREGROUND;
    argsInst->debug_level = LOG_LEVEL_DEFAULT;
}


void usage(char *argv0, struct Args_inst *argsInst) {
    fprintf(stderr, "Version %s \n", VERSION);
    fprintf(stderr, "Usage: %s -d %s [-D%d] [-b] [ip:port or unix:/path/to/socket]  \n\n",
            argv0, argsInst->v4l2loopback_dev,
            argsInst->debug_level,
            argsInst->ip_addr, argsInst->ip_port );

    fprintf(stderr,"Options: \n");
    fprintf(stderr, "\t   | --help          Print this message \n");
    fprintf(stderr, "\t-d | --device        v4l2loopback device \n");
    //fprintf(stderr, "\t-w | --width         Frame width resolution [320..1920] \n");
    //fprintf(stderr, "\t-h | --height        Frame height resolution [240..1080]\n");
    //fprintf(stderr, "\t-f | --frate         Framerate [5..30] \n");
    fprintf(stderr, "\t-b | --background    Run in background mode \n");
    fprintf(stderr, "\t-D | --debug         Debug level [0..5] \n");
}


int pars_args(int argc, char **argv, struct Args_inst *argsInst)
{
   set_defaults(argsInst);

    if( argc == 1 ) {
        usage(argv[0], argsInst);
        exit(0);
    }

    for (;;) {
        int idx;
        int c;
        c = getopt_long(argc, argv, short_options, long_options, &idx);

        if( c == -1 )
            break;

        switch (c) {
            case 0: // getopt_long() flag
                break;

            case 'd':
                strcpy(argsInst->v4l2loopback_dev, optarg);
                break;

            case '?':
                usage(argv[0], argsInst);
                exit(0);

            case 'i':
                //argsInst->get_info = 1;
                break;

            case 'w':
                argsInst->width = strtol(optarg, NULL, 10);
                if (argsInst->width < 320 || argsInst->width > 1920) {
                    log_fatal("A problem with parameter '--width'");
                    return -1;
                }
                break;

            case 'h':
                argsInst->height = strtol(optarg, NULL, 10);
                if (argsInst->height < 240 || argsInst->height > 1080) {
                    log_fatal("A problem with parameter '--height'");
                    return -1;
                }
                break;

            case 'F':
                log_fatal("'--file' option is not implemented yet");
                return -1;
                break;

            case 'f':
                argsInst->frame_rate = strtol(optarg, NULL, 10);
                if (argsInst->frame_rate < 5 || argsInst->frame_rate > 30) {
                    log_fatal("A problem with parameter '--frate'");
                    return -1;
                }
                break;

            case 'c':
                argsInst->frame_count = strtol(optarg, NULL, 10);
                if (argsInst->frame_count < 0 || argsInst->frame_count > 10000) {
                    log_fatal("A problem with parameter '--count'");
                    return -1;
                }
                break;

            case 'D':
            {
                int loglevel = strtol(optarg, NULL, 10);
                if (loglevel < LOG_TRACE || loglevel > LOG_FATAL) {
                    log_fatal("A problem with parameter '-D'");
                    return -1;
                }
                log_set_level(loglevel);
                break;
            }

            case 'b':
                argsInst->run_mode = BACKGROUND;
                //log_set_quiet(BACKGROUND);
                break;

            default:
                usage(argv[0], argsInst);
                exit(0);
        }
    }


    // Parse IP address and port
    char *ip_addr_str = argv[argc-1];

    char *colon_ptr = strchr(ip_addr_str, ':');
    if (!colon_ptr) {
        log_fatal("A problem with parameter IP:port or unix:path");
        return -1;
    }

    *colon_ptr = '\0';


    if (strncmp(ip_addr_str, "unix", 4) == 0) {
        // Unix socket case
        argsInst->ip_unix = colon_ptr + 1;

        log_info("Run as:\n\t%s -d %s -D%d %s unix:%s \n",
                 argv[0], argsInst->v4l2loopback_dev,
                 argsInst->debug_level,
                 (argsInst->run_mode == BACKGROUND) ? "-b" : "",
                 argsInst->ip_unix);

    } else {
        // IP:port case
        strcpy(argsInst->ip_addr, ip_addr_str);
        argsInst->ip_port = strtol(colon_ptr + 1, NULL, 10);

        if (strcmp(argsInst->ip_addr, "*") == 0) {
            strcpy(argsInst->ip_addr, "0.0.0.0");
        }

        if (argsInst->ip_port < 1024 || argsInst->ip_port > 65535) {
            log_fatal("A problem with parameter '--port'");
            return -1;
        }

        log_info("Run as:\n\t%s -d %s -D%d %s %s:%d \n",
                 argv[0], argsInst->v4l2loopback_dev,
                 argsInst->debug_level,
                 (argsInst->run_mode == BACKGROUND) ? "-b" : "",
                 argsInst->ip_addr, argsInst->ip_port);
    }



    return  0;
}

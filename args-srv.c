#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <getopt.h>
#include "libuvc/libuvc.h"

#include "common.h"
#include "log.h"
#include "args.h"

#define FRAME_FORMAT_DEFAULT  UVC_FRAME_FORMAT_MJPEG

const char short_options[] = "d:?iP:D:b:t:v:";

const struct option
        long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "help",   no_argument,       NULL, '?' },
        { "info",   no_argument,       NULL, 'i' },
        { "port",   required_argument, NULL, 'P' },
        { "video",  required_argument, NULL, 'v' },
        { "vtype",  required_argument, NULL, 't' },
        { "count",  required_argument, NULL, 'c' },
        { "debug",  required_argument, NULL, 'D' },
        { "background",  required_argument, NULL, 'b' },
        { 0, 0, 0, 0 }
};


static void set_defaults(struct Args_inst *argsInst )
{
    log_set_level(LOG_LEVEL_DEFAULT);

    //strcpy(argsInst->webcam_dev_name, VIDEO_DEV_DEFAULT);
    argsInst->width = FRAME_W_DEFAULT;
    argsInst->height = FRAME_H_DEFAULT;
    argsInst->frame_rate = FRAME_RATE_DEFAULT;

    argsInst->frame_format = FRAME_FORMAT_DEFAULT;
    strcpy(argsInst->frame_format_str, "MJPEG");

    strcpy(argsInst->ip_addr, IP_ADDR_DEFAULT);
    argsInst->ip_port = IP_PORT_DEFAULT;
    argsInst->run_mode = FOREGROUND;
    argsInst->debug_level = LOG_LEVEL_DEFAULT;
    argsInst->get_info = 0;
}


void usage(char *argv0, struct Args_inst *argsInst) {
    fprintf(stderr, "Version %s \n", VERSION);
    fprintf(stderr, "Usage: %s  --video %dx%dx%d -t %s [-D%d] [-b] %s:%d  \n\n",
            argv0,
            argsInst->width, argsInst->height, argsInst->frame_rate,
            argsInst->frame_format_str,
            argsInst->debug_level,
            argsInst->ip_addr, argsInst->ip_port );

    fprintf(stderr,"Options: \n");
    fprintf(stderr, "\t   | --help          Print this message \n");
    fprintf(stderr, "\t-i | --info          Get webcam info \n");
    fprintf(stderr, "\t-v | --video         Video in format Width x Heihgt x Framerate \n");
    fprintf(stderr, "\t-t | --type          Video type: MJPEG or H264  \n");
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
                //strcpy(argsInst->webcam_dev_name, optarg);
                break;

            case '?':
                usage(argv[0], argsInst);
                exit(0);

            case 'i':
                argsInst->get_info = 1;
                break;

            case 'F':
                log_fatal("'--file' option is not implemented yet");
                return -1;
                break;

            case 't':
                if( strcmp(optarg, "MJPEG") == 0 ) {
                    argsInst->frame_format = UVC_FRAME_FORMAT_MJPEG;
                    strcpy(argsInst->frame_format_str, "MJPEG");
                } else if (strcmp(optarg, "H264") == 0) {
                    log_fatal("Support 'H264' format not implemented yet");
                    return -1;
                } else {
                    log_fatal("A problem with parameter '--video type'");
                    return -1;
                }
                break;

            case 'v':
            {
                const char s[2] = "x";
                char *token;

                if( strlen(optarg) > 80 ) {
                    log_fatal("A problem with parameter '--video'");
                    return -1;
                }
                strcpy(argsInst->video_size, optarg);
                //log_warn("Video: %s", argsInst->video_size);

                /* get the first token */
                token = strtok(argsInst->video_size, s);
                argsInst->width = strtol(token, NULL, 10);
                if (argsInst->width < 320 || argsInst->width > 1920) {
                    log_fatal("A problem with Video frame 'Width'");
                    return -1;
                }

                /* walk through other tokens */
                int i;
                for (i = 0; i < 2; i++) {
                    token = strtok(NULL, s);
                    if (token == NULL) {
                        log_fatal("A problem with parameter '--video'");
                        return -1;
                    }

                    if( i == 0 ) {
                        argsInst->height = strtol(token, NULL, 10);
                        if (argsInst->height < 240 || argsInst->height > 1080) {
                            log_fatal("A problem with parameter '--height'");
                            return -1;
                        }
                    }

                    if( i == 1 ) {
                        argsInst->frame_rate = strtol(token, NULL, 10);
                        if (argsInst->frame_rate < 5 || argsInst->frame_rate > 30) {
                            log_fatal("A problem with parameter '--frate'");
                            return -1;
                        }

                    }
                }

                break;
            }

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

    if( argsInst->get_info == 1 )
        return 0;

    // Parse IP address and port
    char *ip_addr_str = argv[argc-1];

    char *colon_ptr = strchr(ip_addr_str, ':');
    if (!colon_ptr) {
        log_fatal("A problem with parameter IP:port");
        return -1;
    }
    *colon_ptr = '\0';

    strcpy(argsInst->ip_addr, ip_addr_str);
    argsInst->ip_port = strtol(colon_ptr + 1, NULL, 10);

    if (strcmp(argsInst->ip_addr, "*") == 0) {
        strcpy(argsInst->ip_addr, "0.0.0.0");
    }

    if (argsInst->ip_port < 1024 || argsInst->ip_port > 65535) {
            log_fatal("A problem with parameter '--port'");
            return -1;
    }
    // Parse IP address and port


    log_info("Run as:\n\t%s %dx%dx%d -t %s -D%d %s %s:%d \n",
             argv[0],
             argsInst->width, argsInst->height, argsInst->frame_rate,
             argsInst->frame_format_str,
             argsInst->debug_level,
             (argsInst->run_mode == BACKGROUND) ? "-b":"",
             argsInst->ip_addr, argsInst->ip_port );
    if (argsInst->run_mode == BACKGROUND)
        log_info("Log file: %s", LOG_FILE_NAME);

    return  0;
}

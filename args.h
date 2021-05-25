#ifndef INCLUDE_ARGS_H
#define INCLUDE_args_H

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>


struct Args_inst {
    char webcam_dev_name[50];
    char *addr_str;
    char ip_addr[15];       // Max length of 255.255.255.255
    uint32_t ip_port;
    uint32_t width;
    uint32_t height;
    uint16_t frame_rate;
    uint32_t frame_count;
    uint32_t run_mode;
    uint32_t debug_level;
};


int pars_args(int argc, char **argv, struct Args_inst *);


#endif
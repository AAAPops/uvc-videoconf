#ifndef INCLUDE_ARGS_H
#define INCLUDE_args_H


struct Args_inst {
    char v4l2loopback_dev[50];
    char ip_addr[15];       // Max length of 255.255.255.255
    char *ip_unix;
    uint32_t ip_port;
    uint32_t width;
    uint32_t height;
    uint16_t frame_rate;
    uint32_t frame_count;
    uint32_t run_mode;
    uint32_t debug_level;
    uint32_t get_info;
    char video_size[80];
    uint32_t frame_format;
    char frame_format_str[10];
};


int pars_args(int argc, char **argv, struct Args_inst *);


#endif
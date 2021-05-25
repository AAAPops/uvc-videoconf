//
// Created by urv on 5/20/21.
//
#include "log.h"

#ifndef _COMMON_H
#define _COMMON_H

#define VERSION      "0.0.1"

#define FRAME_HEADER_MAGIC  0xFAFA
#define FRAME_HEADER_SZ  20

#define VIDEO_DEV_DEFAULT   "/dev/video0"

#define FRAME_W_DEFAULT     1024    //    800
#define FRAME_H_DEFAULT     576     //448 600
#define FRAME_RATE_DEFAULT  30      // 5 10...
#define FRAMES_CNT_DEFAULT  100

#define IP_ADDR_DEFAULT  "127.0.0.1"
#define IP_PORT_DEFAULT  5100

// LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL
#define LOG_LEVEL_DEFAULT  LOG_INFO


#define FOREGROUND 0
#define BACKGROUND 1

#endif //_COMMON_H

/*
 * Copy a YUV4MPEG stream to a v4l2 output device.
 * The stream is read from standard input.
 * The device can be specified as argument; it defaults to /dev/video0.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
//#include <poll.h>
#include <netinet/in.h>

#include "common.h"
#include "utils.h"
#include "log.h"
#include "tcp_connection.h"
#include "args.h"


struct Frame_hdr {
    uint16_t    magic;      //** Header magic start bytes   0
    uint32_t    width;      //** Frame width                2
    uint32_t    height;     //** Frame height               6
    uint16_t    frate;      //** Frame rate                 10
    uint32_t    size;       //** Frame size in bytes        12
    uint32_t    sequence;   //** Frame number               16
} frameHdr;


uint8_t in_buff[300000];



int read_header(int client_fd, uint32_t runMode)
{
    int ret;
    ssize_t bytes_read;
    uint8_t hdr_buff[FRAME_HEADER_SZ];

    bytes_read = read_tcp_data(client_fd, hdr_buff, FRAME_HEADER_SZ);
    if (bytes_read != FRAME_HEADER_SZ) {
        log_warn("Was not able to read frame header. Finish job");
        return -1;
    }

    frameHdr.magic = htons(*(uint16_t*)(hdr_buff + 0));
    frameHdr.width = htonl(*(uint32_t*)(hdr_buff + 2));
    frameHdr.height = htonl(*(uint32_t*)(hdr_buff + 6));
    frameHdr.frate = htons(*(uint16_t*)(hdr_buff + 10));
    frameHdr.size = htonl(*(uint32_t*)(hdr_buff + 12));
    frameHdr.sequence = htonl(*(uint32_t*)(hdr_buff + 16));

    log_trace("[#%d] Magic: 0x%X,  X: %d,  Y: %d, Rate: %d,  Size: %d",
                      frameHdr.sequence, frameHdr.magic,
                      frameHdr.width, frameHdr.height,
                      frameHdr.frate, frameHdr.size);

    if( runMode == FOREGROUND && log_get_level() == LOG_INFO ) {
        fprintf(stdout, "%05d\b\b\b\b\b", frameHdr.sequence);
        fflush(stdout);
    }

    return 0;
}



int read_jpeg_data(int client_fd) {
    ssize_t bytes_read;

    bytes_read = read_tcp_data(client_fd, in_buff, frameHdr.size);
    log_debug("Was Real read = %d", bytes_read);
    if (bytes_read != frameHdr.size) {
        log_warn("Was not able to read jpeg data. Finish job");
        return -1;
    }

    return 0;
}

int feed_v4l2_dev(int v4l2dev_fd) {
    //write(STDOUT_FILENO, in_buff, frameHdr.size);

    write(v4l2dev_fd, in_buff, frameHdr.size);

    return 0;
}

int copy_frames(int clnt_fd, int v4l2_fd, uint32_t run_mode)
{
    int ret;


    while (1)
    {
        ret = read_header(clnt_fd, run_mode);
        if (ret)
            return -1;

        ret = read_jpeg_data(clnt_fd);
        if (ret)
            return -1;

        ret = feed_v4l2_dev(v4l2_fd);
        if (ret)
            return -1;
    }

    return 0;
}

#define vidioc(op, arg) \
	if (ioctl(v4l2dev_fd, VIDIOC_##op, arg) == -1) \
		sysfail(#op); \
	else


int open_v4l2loopback_dev(char *device, uint32_t frameX, uint32_t frameY)
{
    struct v4l2_format v;
    int fd;

    fd = open(device, O_RDWR);
    if (fd == -1)
       return -1;

    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    //vidioc(G_FMT, &v);
    if (ioctl(fd, VIDIOC_G_FMT, &v) == -1) {
        log_fatal("ioctl(fd, VIDIOC_G_FMT");
        return -1;
    }

    v.fmt.pix.width = frameX;
    v.fmt.pix.height = frameY;
    v.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    v.fmt.pix.sizeimage = frameX * frameY * 3;
    //vidioc(S_FMT, &v);
    if (ioctl(fd, VIDIOC_S_FMT, &v) == -1) {
        log_fatal("ioctl(fd, VIDIOC_G_FMT");
        return -1;
    }

    return fd;
}


int make_handshake(int fd)
{
    int ret;

    ret =  read_header(fd, 1);
    if (ret)
        return -1;
    log_info("make_handshake() = %dx%dx%d", frameHdr.width, frameHdr.height, frameHdr.frate);

    return 0;
}

int main(int argc, char **argv)
{
    int ret;
    int client_fd;
    int v4l2loopback_fd;

    struct Args_inst argsInst;
    MEMZERO(argsInst);
    pars_args(argc, argv, &argsInst);

    if (argsInst.ip_unix) {
        client_fd = connect_to_unix_socket(argsInst.ip_unix);
    } else {
        client_fd = connect_to_srv(argsInst.ip_addr, argsInst.ip_port);
    }
    if (client_fd < 0)
        return -1;

    ret = make_handshake(client_fd);
    if (ret < 0 ) {
        log_fatal("make_handshake()");
        goto err;
    }

    v4l2loopback_fd = open_v4l2loopback_dev(argsInst.v4l2loopback_dev,
                                            frameHdr.width, frameHdr.height);
    if (v4l2loopback_fd < 0) {
        ret = -1;
        goto err;
    }


    ret = copy_frames(client_fd, v4l2loopback_fd, argsInst.run_mode);
    if (ret < 0) {
      goto err;
    }

err:
    client_close(client_fd);
    return 0;
}

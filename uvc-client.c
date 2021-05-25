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
#include <poll.h>
#include <netinet/in.h>

#include "log.h"
#include "tcp_connection.h"
#include "common.h"
#include "args.h"

#define FRAME_X   FRAME_W_DEFAULT
#define FRAME_Y   FRAME_H_DEFAULT

#define SRV_ADDR  "192.168.1.123"
#define SRV_PORT  5100


struct Frame_hdr {
    uint16_t    magic;      //** Header magic start bytes   0
    uint32_t    width;      //** Frame width                2
    uint32_t    height;     //** Frame height               6
    uint16_t    frate;      //** Frame rate                 10
    uint32_t    size;       //** Frame size in bytes        12
    uint32_t    sequence;   //** Frame number               16
} frameHdr;

char *prog;
char *device;
int v4l2dev_fd;
int client_fd;

uint8_t in_buff[300000];


void usage(void)
{
	fprintf(stderr, "Usage: %s [/dev/videoN]\n", prog);
	exit(1);
}

void process_args(int argc, char **argv)
{
	prog = argv[0];
	switch (argc) {
	case 1:
		device = "/dev/video0";
		break;
	case 2:
		device = argv[1];
		break;
	default:
		usage();
		break;
	}
}

void sysfail(char *msg)
{
	perror(msg);
	exit(1);
}

void fail(char *msg)
{
	fprintf(stderr, "%s: %s\n", prog, msg);
	exit(1);
}





int read_header()
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

    log_debug("[#%d] Magic: 0x%X,  X: %d,  Y: %d, Rate: %d,  Size: %d",
                      frameHdr.sequence, frameHdr.magic,
                      frameHdr.width, frameHdr.height,
                      frameHdr.frate, frameHdr.size);

    return 0;
}



int read_jpeg_data(void) {
    ssize_t bytes_read;

    bytes_read = read_tcp_data(client_fd, in_buff, frameHdr.size);
    log_debug("Was Real read = %d", bytes_read);
    if (bytes_read != frameHdr.size) {
        log_warn("Was not able to read jpeg data. Finish job");
        return -1;
    }

    return 0;
}

int feed_v4l2_dev() {
    //write(STDOUT_FILENO, in_buff, frameHdr.size);

    write(v4l2dev_fd, in_buff, frameHdr.size);

    return 0;
}

int copy_frames()
{
    int ret;


	while (1)
    {
        ret = read_header();
        if (ret)
            return -1;

        ret = read_jpeg_data();
        if (ret)
            return -1;

        ret = feed_v4l2_dev();
        if (ret)
            return -1;

        //usleep(1000000/30);

        //return 0;
	}

    return 0;
}

#define vidioc(op, arg) \
	if (ioctl(v4l2dev_fd, VIDIOC_##op, arg) == -1) \
		sysfail(#op); \
	else


int open_video(void)
{
    struct v4l2_format v;

    v4l2dev_fd = open(device, O_RDWR);
    if (v4l2dev_fd == -1)
        sysfail(device);

    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    vidioc(G_FMT, &v);

    v.fmt.pix.width = FRAME_X;
    v.fmt.pix.height = FRAME_Y;
    v.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    v.fmt.pix.sizeimage = FRAME_X * FRAME_Y * 3;
    vidioc(S_FMT, &v);

    return 0;
}

int main(int argc, char **argv)
{
    int ret;
    log_set_level(LOG_LEVEL_DEFAULT);

    client_fd = connect_to_srv(SRV_ADDR, SRV_PORT);

    process_args(argc, argv);
    open_video();
    copy_frames();

    client_close(client_fd);
    return 0;
}

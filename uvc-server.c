#include "libuvc/libuvc.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common.h"
#include "utils.h"
#include "log.h"
#include "tcp_connection.h"
#include "args.h"

struct Frame_hdr {
    uint16_t magic;    //** Header magic start bytes
    uint32_t width;    //** Frame width
    uint32_t height;   //** Frame height
    uint16_t frate;    //** Frame rate
    uint32_t size;     //** Frame size in bytes
    uint32_t sequence; //** Frame number
};

struct cb_param {
    //FILE *write_ptr;
    int client_fd;
    uint16_t frate;
    int cb_err;
    int run_mode;
};

/* This callback function runs once per frame. Use it to perform any
 * quick processing you need, or have it put the frame into your application's
 * input queue. If this function takes too long, you'll start losing frames. */
void cb(uvc_frame_t *frame, void *ptr)
{
    int ret;

    struct cb_param *cbParam = (struct cb_param *) ptr;
    uint8_t hdr_buff[FRAME_HEADER_SZ];

    uint16_t magic = FRAME_HEADER_MAGIC;
    uint16_t frate = cbParam->frate;

    *(uint16_t *) (hdr_buff + 0) = htons(magic);
    *(uint32_t *) (hdr_buff + 2) = htonl(frame->width);
    *(uint32_t *) (hdr_buff + 6) = htonl(frame->height);
    *(uint16_t *) (hdr_buff + 10) = htons(frate);
    *(uint32_t *) (hdr_buff + 12) = htonl(frame->data_bytes);
    *(uint32_t *) (hdr_buff + 16) = htonl(frame->sequence);

    ret = write_tcp_data(cbParam->client_fd, hdr_buff, sizeof(hdr_buff));
    if (ret < 0){
        cbParam->cb_err = -1;
        return;
    }

    ret = write_tcp_data(cbParam->client_fd, frame->data, frame->data_bytes);
    if (ret < 0){
        cbParam->cb_err = -1;
        return;
    }

    log_trace("[#%d] Magic: 0x%X,  X: %d,  Y: %d, Rate: %d,  Size: %d",
              frame->sequence, magic, frame->width, frame->height, frate,
              frame->data_bytes);


    if( cbParam->run_mode == FOREGROUND ) {
        fprintf(stdout, "%05d\b\b\b\b\b", frame->sequence);
        fflush(stdout);
    }
}

int make_handshake(int fd, uint32_t frameW, uint32_t frameH, uint32_t frameR)
{
    int ret;
    uint8_t hdr_buff[FRAME_HEADER_SZ];

    uint16_t magic = FRAME_HEADER_MAGIC;

    *(uint16_t *) (hdr_buff + 0) = htons(magic);
    *(uint32_t *) (hdr_buff + 2) = htonl(frameW);
    *(uint32_t *) (hdr_buff + 6) = htonl(frameH);
    *(uint16_t *) (hdr_buff + 10) = htons(frameR);
    *(uint32_t *) (hdr_buff + 12) = 0;
    *(uint32_t *) (hdr_buff + 16) = 0;

    ret = write_tcp_data(fd, hdr_buff, sizeof(hdr_buff));
    if (ret < 0)
        return -1;

    return 0;
}

void print_webcam_info(void);

int run_as_daemon(void) {
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if( pid < 0 ) {
        log_fatal("fork() [%m]");
        exit(EXIT_FAILURE);;
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) { // Parent process has finished executing
        log_debug("Parent process has finished executing");
        exit(0);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */
    FILE *log_fp;
    log_fp = fopen(LOG_FILE_NAME, "w");
    if (log_fp) {
        log_set_fp(log_fp);
    } else {
        log_warn("fopen() [%m]");
        log_warn("Continue without logging into file '%s'", LOG_FILE_NAME);
    }


    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        log_fatal("setsid() [%m]");
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if( chdir("/") < 0 ) {
        log_fatal("chdir() [%m]");
        exit(EXIT_FAILURE);
    }

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Daemon-specific initialization goes here */
    log_set_quiet(BACKGROUND);

    return 0;
}


int main(int argc, char **argv) {
    int ret;
    uvc_context_t *ctx;
    uvc_device_t *dev;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_error_t uvc_res;
    enum uvc_req_code req_code;

    struct cb_param cbParam;
    MEMZERO(cbParam);

    struct Args_inst argsInst;
    MEMZERO(argsInst);
    pars_args(argc, argv, &argsInst);

    if (argsInst.get_info == 1)
        print_webcam_info();

    cbParam.frate = argsInst.frame_rate;

    if( argsInst.run_mode == BACKGROUND) {
        ret = run_as_daemon();
        if (ret < 0)
            return -1;
    }

    int srv_fd = srv_start(argsInst.ip_addr, argsInst.ip_port);
    if (srv_fd < 0) {
        log_fatal("srv_start()");
        return -1;
    }

    while (1)
    {
        cbParam.cb_err = 0;
        cbParam.run_mode = argsInst.run_mode;

        cbParam.client_fd =
                srv_client_accept(srv_fd, argsInst.ip_addr, argsInst.ip_port);
        if (cbParam.client_fd < 0) {
            log_fatal("srv_client_accept()");
            srv_close(srv_fd);
            return -1;
        }

        ret = make_handshake(cbParam.client_fd, argsInst.width,
                             argsInst.height, argsInst.frame_rate);
        if (ret < 0 ) {
            log_fatal("make_handshake()");
            goto err1;
        }

        /* Initialize a UVC service context. Libuvc will set up its own libusb
         * context. Replace NULL with a libusb_context pointer to run libuvc
         * from an existing libusb context. */
        uvc_res = uvc_init(&ctx, NULL);
        if (uvc_res < 0) {
            log_fatal("uvc_init");
            return -1;
        }
        log_info("UVC initialized()");

        /* Locates the first attached UVC device, stores in dev */
        uvc_res = uvc_find_device(
                ctx, &dev, 0, 0,
                NULL); /* filter devices: vendor_id, product_id, "serial_num" */
        if (uvc_res < 0) {
            log_fatal("uvc_find_device()"); /* no devices found */
            return -1;
        }
        log_info("Webcam found");

        /* Try to open the device: requires exclusive access */
        uvc_res = uvc_open(dev, &devh);
        if (uvc_res < 0) {
            log_fatal("uvc_open()"); /* unable to open device */
            return -1;
        }
        log_info("Webcam opened");

        /* Print out a message containing all the information that libuvc
         * knows about the device */
        //uvc_print_diag(devh, stderr);

        /* Try to negotiate a Width:Heigh:Fps MJPEG stream profile */
        uvc_res = uvc_get_stream_ctrl_format_size(
                devh, &ctrl,            /* result stored in ctrl */
                UVC_FRAME_FORMAT_MJPEG, /* YUV 422, aka YUV 4:2:2. try _COMPRESSED */
                argsInst.width, argsInst.height,
                argsInst.frame_rate /* width, height, fps */
        );

        if (uvc_res < 0) {
            log_fatal("uvc_get_stream_ctrl_format_size()"); /* device doesn't provide a matching stream */
            goto err1;
        }
        /* Print out the result */
        if( log_get_level() <= LOG_INFO ) {
            log_info("\n---------- Webcam controls ----------");
            uvc_print_stream_ctrl(&ctrl, stderr);
            log_info("\n-------------------------------------");
        }

        /* Start the video stream. The library will call user function cb:
        *   cb(frame, (void*) 12345)
        */
        uvc_res = uvc_start_streaming(devh, &ctrl, cb, &cbParam, 0);
        if (uvc_res < 0) {
            log_fatal("start_streaming()"); /* unable to start stream */
            goto err1;
        }
        log_info("Webcam streaming...");

        uvc_set_ae_mode(devh, 8); // e.g., turn on auto exposure
        uint8_t ae_mode;
        uvc_get_ae_mode(devh, &ae_mode, UVC_GET_CUR);
        log_trace("-----> ae_mode = %d", ae_mode);

        while (cbParam.cb_err >= 0) {
            usleep(1000);
        }
        log_fatal("cb_err = %d", cbParam.cb_err);

        /* End the stream. Blocks until last callback is serviced */
        uvc_stop_streaming(devh);
        log_info("Webcam done streaming");

    err1:
        /* Release our handle on the device */
        uvc_close(devh);
        log_info("Webcam closed");

        /* Release the device descriptor */
        uvc_unref_device(dev);

        /* Close the UVC context. This closes and cleans up any existing device handles, and it closes the libusb context if one was not provided. */
        uvc_exit(ctx);
        log_info("UVC exited");

        client_close(cbParam.client_fd);
    }

    srv_close(srv_fd);
    log_info("Server stop");

    return 0;
}

void print_webcam_info(void)
{
    int ret;
    uvc_context_t *ctx;
    uvc_device_t *dev;
    uvc_device_handle_t *devh;
    uvc_error_t uvc_res;

    /* Initialize a UVC service context. Libuvc will set up its own libusb
         * context. Replace NULL with a libusb_context pointer to run libuvc
         * from an existing libusb context. */
    uvc_res = uvc_init(&ctx, NULL);
    if (uvc_res < 0) {
        log_fatal("uvc_init");
        exit(-1);
    }
    log_info("UVC initialized()");

    /* Locates the first attached UVC device, stores in dev */
    uvc_res = uvc_find_device(
            ctx, &dev, 0, 0,
            NULL); /* filter devices: vendor_id, product_id, "serial_num" */
    if (uvc_res < 0) {
        log_fatal("uvc_find_device()"); /* no devices found */
        exit(-1);
    }
    log_info("Webcam found");

    /* Try to open the device: requires exclusive access */
    uvc_res = uvc_open(dev, &devh);
    if (uvc_res < 0) {
        log_fatal("uvc_open()"); /* unable to open device */
        exit(-1);
    }
    log_info("Webcam opened");

    /* Print out a message containing all the information that libuvc
    * knows about the device */
    uvc_print_diag(devh, stderr);


    /* Release our handle on the device */
    uvc_close(devh);
    log_info("Webcam closed");

    /* Release the device descriptor */
    uvc_unref_device(dev);

    /* Close the UVC context. This closes and cleans up any existing device handles, and it closes the libusb context if one was not provided. */
    uvc_exit(ctx);
    log_info("UVC exited");

    exit(0);
}
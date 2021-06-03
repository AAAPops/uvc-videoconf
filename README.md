#### uvc-videoconf


Project build on top of [libuvc](https://github.com/libuvc/libuvc) and [v4l2loopback](https://github.com/umlaeute/v4l2loopback)

You can redirect remote webcam to local mashine and use it in familiar manner with the same programs as always: VLC player, Gstreamer etc.


##### On Server side with attached Webcam:

In my case I use small computer based on SoC i.mx6Q (**libuvc** has to be installed)

```bash
$ mkdir arm-build && cd arm-build
$ cmake ../
$ make

$ ./uvc-server -v 1024x576x24 -t MJPEG -D2  *:5100
  
    Server listen on all interfaces and will push MJPEG videostream 1024x576x24
    to client as soon as it connects to the server
    You can run Server as a daemon.
```

##### On Client side with 'v4l2loopback' device on it :

In my case this is ordinary x86 notebook (**v4l2loopback** Kernel module has to be load)

```bash
$ mkdir x86-build && cd x86-build
$ cmake ../
$ make

$ ./uvc-client -d /dev/video0  192.168.1.99:5100

    '/dev/video0' is v4l2loopback device
    Now you can use VLC player with this Capture device 
    or
    gst-launch-1.0 -v v4l2src device=/dev/video0 ! jpegparse ! jpegdec ! videoconvert ! xvimagesink sync=false 
```

##### ToDo

- [ ] _Work with h264 stream from webcam_
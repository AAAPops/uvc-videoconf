cmake_minimum_required(VERSION 3.13)
project(webcam_264_proto)

set(CMAKE_C_STANDARD 99)

#set(SOURCE          main.c args.c webcam.c server.c coda960.c)
#set(HEADER common.h        args.h webcam.h server.h coda960.h)

#find_library (JPEG_LIB jpeg)
#message ("JPEG_LIB=${JPEG_LIB}")

find_library (UVC_LIB uvc)
message ("UVC_LIB=${UVC_LIB}")


add_definitions(-DLOG_USE_COLOR)

add_executable(uvc-server uvc-server.c
        tcp_connection.c tcp_connection.h
        log.c utils.c args-srv.c)

#add_executable(test test.c         ../log.c)

add_executable(uvc-client  uvc-client.c
        tcp_connection.c tcp_connection.h
        log.c utils.c args-clnt.c)

target_link_libraries(uvc-server ${UVC_LIB})

target_link_libraries(uvc-client ${UVC_LIB} )


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "log.h"
#include "tcp_connection.h"
#include "utils.h"

#define TIMEOUT_SEC  5 * 1000

#define MEMZERO(x)	memset(&(x), 0, sizeof (x));


int srv_start(const char *addr_str, int port)
{
    int srv_fd;
    struct sockaddr_in servaddr;
    socklen_t peer_addr_size;


    struct in_addr inp;
    inet_aton(addr_str, &inp);

    // socket create and verification
    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if( srv_fd == -1 ) {
        log_fatal("Socket creation failed...[%m]");
        return -1;
    }

    int enable = 1;
    if( setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0 ) {
        log_fatal("setsockopt(SO_REUSEADDR) failed");
        return -1;
    }

    MEMZERO(servaddr);
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    //servaddr.sin_addr.s_addr = htonl(addr); //INADDR_LOOPBACK for 127.0.0.1
    servaddr.sin_addr.s_addr = inp.s_addr;
    servaddr.sin_port = htons(port);

    // Binding newly created socket to given IP and verification
    if ((bind(srv_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) {
        log_fatal("Socket bind failed... [%m]");
        return  -1;;
    }

    // Now server is ready to listen and verification
    if( listen(srv_fd, 1) != 0 ) {
        log_fatal("Server listen failed... [%m]");
        return -1;
    }

    return srv_fd;
}

int srv_client_accept(int srv_fd, const char *addr_str, int port) {
    int client_fd;
    log_info("Server waiting for a client on %s:%d...", addr_str, port);

    // Accept the data packet from client and verification
    client_fd = accept(srv_fd, (struct sockaddr*)NULL, NULL);
    if( client_fd < 0 ) {
        log_fatal("Client acccept failed... [%m]");
        return -1;
    }

    log_info("Server acccept the client...");

    return client_fd;
}


void srv_close(int srv_fd) {
    if( close(srv_fd) == -1 )
        log_fatal("Srv: server close()");

    log_info("Server finished successful");
}


int write_tcp_data(int peer_fd, void* buff_ptr, size_t buff_len) {
    // send the buffer to 'stdout'
    //if (file_ptr)
    //    fwrite(buff_ptr, buff_size, 1, file_ptr);

    // send the buffer to client
    int n_bytes = send(peer_fd, buff_ptr, buff_len, MSG_NOSIGNAL);
    if( n_bytes != buff_len ) {
        log_fatal("TCP: Not able to sent %d bytes", buff_len);
        return -1;
    }

    return 0;
}

/*
int srv_get_data(struct Srv_inst* i) {

    int n_bytes = recv(i->peer_fd, i->read_buff, sizeof(i->read_buff), 0);
    if( n_bytes == -1 ) {
        err("Some error with client [%m]");
        return -1;
    } else if( n_bytes == 0 ) {
        info("Client closed connection");
        return 0;
    }

    return n_bytes;
}
*/

int read_tcp_data(int peer_fd, void *buffer, size_t total_to_read) {

    struct pollfd pfds;
    int ret;
    size_t n_bytes;
    size_t offset = 0;
    size_t left_to_read = total_to_read;
    int iter = 0;

    pfds.fd = peer_fd;
    pfds.events = POLLIN;

    //double time_start = stopwatch(NULL, 0);

    while (1) {
        if (left_to_read == 0)
          break;

        ret = poll(&pfds, 1, TIMEOUT_SEC);
        if( ret == -1 ) {
            log_fatal("poll: [%m]");
            return -1;

        } else if( ret == 0 ) {
            log_fatal("poll: Time out");
            return -1;
        }
/*
        printf("  fd=%d; events: %s%s%s%s\n", pfds.fd,
               (pfds.revents & POLLIN)  ? "POLLIN "  : "",
               (pfds.revents & POLLHUP) ? "POLLHUP " : "",
               (pfds.revents & POLLHUP) ? "POLLRDHUP " : "",
               (pfds.revents & POLLERR) ? "POLLERR " : "");
*/
        if (pfds.revents & POLLIN) {
            n_bytes = recv(peer_fd, buffer + offset, left_to_read, 0);
            if( n_bytes == -1 ) {
                log_fatal("recv: [%m]");
                return -1;
            }
            if( n_bytes == 0 ) {
                log_warn("peer closed connection");
                return -1;
            }
        } else {  // POLLERR | POLLHUP
            log_warn("peer closed connection");
            return -1;
        }


        offset += n_bytes;
        left_to_read -= n_bytes;
        log_trace("Iter = %d", ++iter);
    }

    //log_info("total_to_read = %d", total_to_read);
    //stopwatch("Read data time", time_start);
    return total_to_read;
}

//*** Client Functions ***
int connect_to_srv(const char *addr_str, int port) {
    int client_fd;
    struct sockaddr_in servaddr;
    MEMZERO(servaddr);

    struct in_addr inp;
    inet_aton(addr_str, &inp);

    // socket create and verification
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if( client_fd == -1 ) {
        log_fatal("Socket creation failed...");
        return -1;
    }
    log_info("Socket successfully created..");

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inp.s_addr;
    servaddr.sin_port = htons(port);

    // connect the client socket to server socket
    int ret = connect(client_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (ret) {
        log_fatal("Connection with the server failed...");
        return -1;
    }
    log_info("Connected to the server..");

    return client_fd;
}

void client_close(int client_fd) {
    if( close(client_fd) == -1 )
        log_fatal("'Srv: peer close()");

    log_info("Peer closed successful");
}

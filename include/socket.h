#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <mswsock.h> 
    #include <windows.h>
#else
    #include <errno.h>
    #include <sys/socket.h>
    #include <sys/epoll.h>
    #include <netinet/tcp.h> 
    #include <arpa/inet.h>
    #include <unistd.h> 
    #include <fcntl.h>
    #include <ifaddrs.h>
    #include <netdb.h>
#endif

#define BACKLOG SOMAXCONN  // how many pending connections queue will hold
#define PORT_BUFFER_SIZE 64
#define HOSTNAME_BUFFER_SIZE 256

#ifdef _WIN32
    typedef SOCKET socket_handle;
#else
    typedef int socket_handle;
#endif

#ifdef _WIN32
socket_handle create_overlapped_socket(int family, int socktype, int protocol);
int load_winsock_extensions(socket_handle s);
#endif

typedef struct Socket {
    socket_handle sockfd;
    char port[PORT_BUFFER_SIZE];
} Socket;

int socket_init(Socket *sock);
void socket_cleanup();

void socket_close(Socket *sock);

int socket_tcp_socket(Socket *sock, const char *ip, const char *port);
int socket_listen_connection(Socket *sock);
socket_handle socket_accept_connection(Socket *sock);
socket_handle socket_get_handle(const Socket *sock);

char* socket_get_host_ip_addr(char *buffer, size_t bufsize);
char* socket_get_host_name(char *buffer, size_t bufsize);
char* socket_get_ip_addr(struct sockaddr *sa, char *buffer, size_t bufsize);

void socket_set_non_blocking(socket_handle fd);
void socket_set_opt_reuse_addr(socket_handle sockfd, int on);
void socket_set_opt_keep_alive(socket_handle sockfd, int on);
void socket_set_opt_tcp_no_delay(socket_handle sockfd, int on);
void socket_set_opt_tcp_quick_ack(socket_handle sockfd, int on);
void socket_set_opt_tcp_fast_open(socket_handle sockfd, int qlen);
void socket_set_opt_linger(socket_handle sockfd);
void socket_set_opt_rcvbuf(socket_handle sockfd, int bufsize);
void socket_set_opt_sndbuf(socket_handle sockfd, int bufsize);
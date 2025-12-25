#ifndef EVENT_POLL_H
#define EVENT_POLL_H

#include "socket.h"

#define MAX_EVENTS      64
#define EV_BUF_SIZE     8192

#define EVENT_READ      0x01
#define EVENT_WRITE     0x02
#define EVENT_ET        0x04
#define EVENT_ONESHOT   0x08

typedef void (*on_accept_cb)(void *user_data, socket_handle client_fd);
typedef void (*on_receive_cb)(void *user_data, socket_handle fd, const char *buffer, size_t len);
typedef void (*on_send_cb)(void *user_data, socket_handle fd, size_t bytes_sent);
typedef void (*on_disconnect_cb)(void *user_data, socket_handle fd);
typedef void (*on_error_cb)(void *user_data, socket_handle fd, int error_code);

typedef struct {
    on_accept_cb        on_accept;
    on_receive_cb       on_receive;
    on_send_cb          on_send;
    on_disconnect_cb    on_disconnect;
    on_error_cb         on_error;
} event_callbacks_t;

#ifdef _WIN32
    extern LPFN_ACCEPTEX g_AcceptEx;
    extern LPFN_GETACCEPTEXSOCKADDRS g_GetAcceptExSockaddrs;
#endif

typedef struct  
{
#ifdef _WIN32
    HANDLE              iocp;
#else
    int                 epoll_fd;
#endif
    Socket              listener;
    socket_handle       *sockets;
    bool                running;
    event_callbacks_t   *callbacks;
}event_poll_t;

typedef struct 
{
#ifdef _WIN32
    // IOCPContext
    OVERLAPPED      overlapped;             // must be the first member for the CONTAINING_RECORD trick
    WSABUF          wsa_buf;
    char            buffer[EV_BUF_SIZE];
    size_t          send_len;
    int             operation_type;         // 0 = recv, 1 = send, 2 = accept
#endif
    socket_handle   fd;
    event_poll_t    *ep;
    uint32_t        events;
    void            *user_data; // custom ctx
} event_ctx_t;


#ifdef _WIN32
event_poll_t *event_poll_create(const char *ip, const char *port);
void event_poll_destroy(event_poll_t *ep);
int post_recv(event_ctx_t *ctx);
int post_accept(event_poll_t *ep, void *user_data);




#endif

#endif // EVENT_POLL_H

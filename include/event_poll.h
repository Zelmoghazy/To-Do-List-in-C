#ifndef EVENT_POLL_H
#define EVENT_POLL_H

#include "socket.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <sys/epoll.h>
#endif

#define MAX_EVENTS      64

#define EVENT_READ      0x01
#define EVENT_WRITE     0x02
#define EVENT_ET        0x04  // Edge-triggered
#define EVENT_ONESHOT   0x08

typedef struct event_poll_t event_poll_t;

typedef void (*event_callback)(void *user_data, uint32_t events);

struct event_poll_t 
{
    Socket listener;
#ifdef _WIN32
    HANDLE iocp;
    socket_handle *sockets;
    int socket_count;
    int socket_capacity;
#else
    int epoll_fd;
#endif
    bool running;
};

typedef struct 
{
    void           *user_data; // custom ctx
    event_poll_t   *ep;
    socket_handle  fd;
    event_callback callback;
    uint32_t       events;
} event_ctx_t;

event_poll_t event_poll_create(const char *ip, const char *port);
void event_poll_destroy(event_poll_t *ep);
int event_poll_register_ctx(event_poll_t *ep, socket_handle fd, void *user_data, uint32_t events, event_callback callback);

int event_poll_register(event_poll_t *ep, socket_handle fd, void *user_data, 
                        uint32_t events, event_callback callback);
int event_poll_modify(event_poll_t *ep, socket_handle fd, uint32_t events);
int event_poll_remove(event_poll_t *ep, socket_handle fd);

void event_poll_loop(event_poll_t *ep, event_callback new_conn_handler);

void event_poll_stop(event_poll_t *ep);

#endif // EVENT_POLL_H

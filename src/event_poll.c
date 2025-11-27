
#include "event_poll.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <errno.h>
#endif

#ifdef _WIN32


typedef struct 
{
    OVERLAPPED overlapped;
    WSABUF wsa_buf;
    char buffer[4096];
    socket_handle fd;
    void *user_data;
    event_callback callback;
} IOCPContext;

event_poll_t *event_poll_create(const char *ip, const char *port) 
{
    event_poll_t *ep = malloc(sizeof(event_poll_t));  // Heap allocation
    if (!ep) {
        fprintf(stderr, "Failed to allocate event_poll_t\n");
        return NULL;
    }
    memset(ep, 0, sizeof(event_poll_t));

    if(socket_init(&ep->listener) < 0) 
    {
        fprintf(stderr, "event_poll_create: Failed to initialize Socket\n");
        free(ep);
        return NULL;
    }

    socket_tcp_socket(&ep->listener, ip, port);
    socket_listen_connection(&ep->listener);

    if (ep->listener.sockfd == INVALID_SOCKET) {
        free(ep);
        return NULL;
    }
    /*
        Create an I/O completion port, and associates a device
        with an I/O completion port. 
     */
    ep->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                      NULL,
                                      0, 
                                      0); // number of concurrent threads, 0 -> as many as host machine CPUs
    if (!ep->iocp) {
        fprintf(stderr, "CreateIoCompletionPort failed: %lu\n", GetLastError());
        socket_close(&ep->listener);
        free(ep);
        return NULL;
    }

    /*
        When you create an I/O completion port, the kernel actually creates five different data structures
        1- a device list indicating the device or devices associated with the port.
        2- an I/O completion queue. 
        3- the waiting thread queue.
        4- a released thread list
        5- the paused thread list
     */

    /* 
        Associate listener with IOCP
        by appending an entry to an existing completion port's device list. 
     */
    CreateIoCompletionPort((HANDLE)socket_get_socket(&ep->listener), // handle of the device
                          ep->iocp, // the handle of an existing completion port
                          (ULONG_PTR)socket_get_socket(&ep->listener), // a completion key (OS doesnt care what)
                          0);

    ep->socket_capacity = 16;
    ep->sockets = (socket_handle*)calloc(ep->socket_capacity, sizeof(socket_handle));
    ep->running = true;

    return ep;
}

void event_poll_destroy(event_poll_t *ep) 
{
    if (!ep) return;

    ep->running = false;
    
    if (ep->sockets) {
        free(ep->sockets);
    }
    
    if (ep->iocp) {
        CloseHandle(ep->iocp);
    }
    
    socket_close(&ep->listener);
    free(ep);
}

int event_poll_register(event_poll_t *ep, socket_handle fd, void *user_data, uint32_t events, event_callback callback) 
{
    if (!ep) return -1;

    // Associate socket with IOCP
    if (CreateIoCompletionPort((HANDLE)fd, ep->iocp, (ULONG_PTR)fd, 0) == NULL) {
        return -1;
    }

    // Track socket
    if (ep->socket_count >= ep->socket_capacity) 
    {
        ep->socket_capacity *= 2;
        ep->sockets = (socket_handle*)realloc(ep->sockets, 
                                              ep->socket_capacity * sizeof(socket_handle));
    }
    ep->sockets[ep->socket_count++] = fd;

    // Post initial recv if reading
    if (events & EVENT_READ) 
    {
        IOCPContext *ctx = (IOCPContext*)calloc(1, sizeof(IOCPContext));
        ctx->fd = fd;
        ctx->user_data = user_data;
        ctx->callback = callback;
        ctx->wsa_buf.buf = ctx->buffer;
        ctx->wsa_buf.len = sizeof(ctx->buffer);

        DWORD flags = 0;
        WSARecv(fd, &ctx->wsa_buf, 1, NULL, &flags, &ctx->overlapped, NULL);
    }

    return 0;
}

int event_poll_modify(event_poll_t *ep, socket_handle fd, uint32_t events) 
{
    return 0;
}

int event_poll_remove(event_poll_t *ep, socket_handle fd) 
{
    if (!ep){
        return -1;
    }

    // Remove from tracked sockets
    for (int i = 0; i < ep->socket_count; i++) 
    {
        if (ep->sockets[i] == fd) 
        {
            ep->sockets[i] = ep->sockets[--ep->socket_count];
            break;
        }
    }

    closesocket(fd);
    return 0;
}

void event_poll_loop(event_poll_t *ep, event_callback new_conn_handler, void *user_data) 
{
    DWORD bytes;
    ULONG_PTR key;
    OVERLAPPED *overlapped;

    while (ep->running) 
    {
        // Check for new connections (non-blocking)
        socket_handle new_fd = socket_accept_connection(&ep->listener);
        if (new_fd != INVALID_SOCKET && new_conn_handler) 
        {
            socket_set_non_blocking(new_fd);
            new_conn_handler(user_data, EVENT_READ);
        }

        // Wait for I/O completions
        // put the calling thread to sleep until an entry appears in the specified completion port's I/O completion queue
        BOOL result = GetQueuedCompletionStatus(ep->iocp, // Monitored completion port
                                                &bytes, // the number of bytes transferred
                                                &key, // the completion key
                                                &overlapped, // he address of the OVERLAPPED structure.
                                                100); // specified time-out
        DWORD dwError = GetLastError();

        if (!result)
        {
            if(overlapped)
            {

            }
            else
            {
                if (dwError == WAIT_TIMEOUT) {
                    // timed out
                }
                else
                {
                    // ??
                }
            }
            continue;
        }

        IOCPContext *ctx = CONTAINING_RECORD(overlapped, IOCPContext, overlapped);

        if (bytes > 0 && ctx->callback) 
        {
            ctx->callback(ctx->user_data, EVENT_READ);
        }

        // Re-post recv if still active
        if (bytes > 0) 
        {
            memset(&ctx->overlapped, 0, sizeof(OVERLAPPED));
            DWORD flags = 0;
            int ret = WSARecv(ctx->fd, &ctx->wsa_buf, 1, NULL, &flags, &ctx->overlapped, NULL);
            if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                closesocket(ctx->fd);
                free(ctx);
            }
        } 
        else 
        {
            closesocket(ctx->fd);
            free(ctx);
        }
    }
}

void event_poll_stop(event_poll_t *ep) 
{
    if (ep) {
        ep->running = false;
    }
}

#else

event_poll_t *event_poll_create(const char *ip, const char *port) 
{
    event_poll_t *ep = malloc(sizeof(event_poll_t));  // Heap allocation
    if (!ep) {
        fprintf(stderr, "Failed to allocate event_poll_t\n");
        return NULL;
    }
    
    memset(ep, 0, sizeof(event_poll_t));

    if (socket_init(&ep->listener) < 0) 
    {
        fprintf(stderr, "event_poll_create : Failed to initialize Socket\n");
        free(ep);
        return NULL;
    }

    socket_tcp_socket(&ep->listener, ip, port);
    socket_listen_connection(&ep->listener);

    // creates a new epoll instance and returns a file descriptor referring to that instance.
    if ((ep->epoll_fd = epoll_create1(0)) < 0) 
    {
        fprintf(stderr, "epoll_create1 failed: %s\n", strerror(errno));
        socket_close(&ep->listener);
        free(ep);
        return NULL;
    }

    event_ctx_t *listener_ctx = malloc(sizeof(event_ctx_t));
    if (!listener_ctx) {
        fprintf(stderr, "Failed to allocate listener context\n");
        close(ep->epoll_fd);
        socket_close(&ep->listener);
        free(ep);
        return NULL;
    }
    memset(listener_ctx, 0, sizeof(event_ctx_t));
    listener_ctx->fd = socket_get_socket(&ep->listener);
    listener_ctx->ep = ep;
    listener_ctx->callback = NULL;
    listener_ctx->user_data = NULL;
    listener_ctx->events = 0;

    // Add listener to epoll interest list, register it in edge triggered mode 
    if(event_poll_register_ctx(ep, listener_ctx, EVENT_READ | EVENT_ET) < 0)
    {
        fprintf(stderr, "epoll_ctl failed: %s\n", strerror(errno));
        close(ep->epoll_fd);
        socket_close(&ep->listener);
        free(listener_ctx);
        free(ep);
        return NULL;
    } 

    ep->running = true;
    return ep;
}

void event_poll_destroy(event_poll_t *ep) 
{
    if (!ep) return;

    ep->running = false;

    if (ep->epoll_fd >= 0) {
        close(ep->epoll_fd);
    }

    socket_close(&ep->listener);
    free(ep);
}

/*
    edge-triggered notifications (EPOLLIN|EPOLLET)
 */
int event_poll_register(event_poll_t *ep, socket_handle fd, uint32_t events) 
{
    if (!ep || ep->epoll_fd < 0) 
    {
        return -1;
    }

    struct epoll_event ev;
    ev.events = 0;
    
    if (events & EVENT_READ) ev.events |= EPOLLIN;          // arrival of new data
    if (events & EVENT_WRITE) ev.events |= EPOLLOUT;        // available to be written into without blocking
    if (events & EVENT_ET) ev.events |= EPOLLET;            // edge-triggered notifications
    if (events & EVENT_ONESHOT) ev.events |= EPOLLONESHOT;  // only once and stop monitoring fd for subsequent occurrences of that event.

    ev.data.fd = fd;

    // registering interest in a particular file descriptor
    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) 
    {
        fprintf(stderr, "epoll_ctl ADD failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int event_poll_register_ctx(event_poll_t *ep, event_ctx_t*ctx, uint32_t events) 
{
    if (!ep || ep->epoll_fd < 0) 
    {
        return -1;
    }

    struct epoll_event ev;
    ev.events = 0;
    
    if (events & EVENT_READ) ev.events |= EPOLLIN;          // arrival of new data
    if (events & EVENT_WRITE) ev.events |= EPOLLOUT;        // available to be written into without blocking
    if (events & EVENT_ET) ev.events |= EPOLLET;            // edge-triggered notifications
    if (events & EVENT_ONESHOT) ev.events |= EPOLLONESHOT;  // only once and stop monitoring fd for subsequent occurrences of that event.

    ev.data.ptr = ctx;

    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_ADD, ctx->fd, &ev) < 0) 
    {
        fprintf(stderr, "epoll_ctl ADD failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int event_poll_modify(event_poll_t *ep, socket_handle fd, uint32_t events) 
{
    if (!ep || ep->epoll_fd < 0) 
    {
        return -1;
    }

    struct epoll_event ev;
    ev.events = 0;
    ev.data.fd = fd;

    if (events & EVENT_READ) ev.events |= EPOLLIN;
    if (events & EVENT_WRITE) ev.events |= EPOLLOUT;
    if (events & EVENT_ET) ev.events |= EPOLLET;
    if (events & EVENT_ONESHOT) ev.events |= EPOLLONESHOT;

    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) 
    {
        fprintf(stderr, "epoll_ctl MOD failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int event_poll_modify_ctx(event_poll_t *ep, event_ctx_t*ctx, uint32_t events) 
{
    if (!ep || ep->epoll_fd < 0) 
    {
        return -1;
    }

    struct epoll_event ev = {0};
    ev.events = 0;
    
    if (events & EVENT_READ) ev.events |= EPOLLIN;
    if (events & EVENT_WRITE) ev.events |= EPOLLOUT;
    if (events & EVENT_ET) ev.events |= EPOLLET;
    if (events & EVENT_ONESHOT) ev.events |= EPOLLONESHOT;

    ev.data.ptr = ctx;

    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_MOD, ctx->fd, &ev) < 0) 
    {
        fprintf(stderr, "epoll_ctl MOD failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int event_poll_remove(event_poll_t *ep, socket_handle fd) 
{
    if (!ep || ep->epoll_fd < 0) {
        return -1;
    }

    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_DEL, fd,NULL) < 0) {
        fprintf(stderr, "epoll_ctl DEL failed: %s\n", strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}

int event_poll_remove_ctx(event_poll_t *ep, event_ctx_t*ctx)
{
    if (!ep || ep->epoll_fd < 0 || !ctx) return -1;

    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_DEL, ctx->fd, NULL) < 0) {
        // if fd already closed, ignore specific errors
        if (errno != ENOENT && errno != EBADF)
            perror("epoll_ctl DEL failed");
    }

    close(ctx->fd);
    free(ctx);
    return 0;
}

void event_poll_handle_client_event(void *user_data, uint32_t events)
{
    char buf[4096]; 

    event_ctx_t*ctx = (event_ctx_t*)user_data;

    if (events & EVENT_READ) 
    {
        for (;;) 
        {
            // ---
            ssize_t n = recv(ctx->fd, buf, sizeof(buf), 0);

            if (n > 0) 
            {

            } 
            else if (n == 0) 
            {
                 // EOF encountered
                event_poll_remove_ctx(ctx->ep, ctx);
                return;
            } 
            else 
            {
                /*
                    With EPOLLET When you get EPOLLIN, you must read until EAGAIN. 
                    If you leave unread data You will not get another event.
                    With EPOLLONESHOT after callback finishes handling the event 
                    (reading everything until EAGAIN), epoll will not send any 
                    more events, we need to rearm it manually its used mainly to
                    make it thread safe and no two threads can handle same fd at 
                    same time, ONESHO ensures only one thread handles the event.
                 */
                if (errno == EAGAIN || errno == EWOULDBLOCK) 
                {
                    // read end normally
                    // drained -> ok to re-arm
                    break;
                } 
                else 
                {
                    // real error
                    perror("read");
                    event_poll_remove_ctx(ctx->ep, ctx);
                    return;
                }
            }
        }
    }
    event_poll_modify_ctx(ctx->ep, ctx, EVENT_READ | EVENT_ET | EVENT_ONESHOT);
}

void event_poll_handle_new_connection(event_poll_t *ep)
{
    // call accept as many times as we can
    for (;;) 
    {
        /*
            A new descriptor is returned by accept 
            for each client that connects to the server. 
        */
        socket_handle new_fd = socket_accept_connection(&ep->listener);

        if (new_fd < 0){
            break;
        }

        // apply non-blocking IO to the connection sockets as well
        socket_set_non_blocking(new_fd);

        event_ctx_t*ctx = malloc(sizeof(event_ctx_t));
        memset(ctx, 0, sizeof(event_ctx_t));

        ctx->fd = new_fd;
        ctx->ep = ep;
        ctx->callback = event_poll_handle_client_event;

        /*
            With edge triggered You get an event only when new data arrives
            (or space becomes available to write). If nothing happened → no event,
            even if the fd is still readable because old data is sitting there. 

            EPOLLONESHOT + EPOLLET turns your fd into a “single-shot trap.”
            Once it fires, it disarms itself, and you must re-enable it manually.

            After delivering this event ONCE, disable further notifications for 
            this fd until you explicitly re-enable it.
         */
        if(event_poll_register_ctx(ep, ctx, EVENT_READ | EVENT_ET | EVENT_ONESHOT)<0)
        {
            close(new_fd);
            free(ctx);
            continue;
        }
    }
}

void event_poll_loop(event_poll_t *ep) 
{
    /*
        typedef union epoll_data {
            void    *ptr;           // Pointer to user-defined data 
            int      fd;            // File descriptor
            uint32_t u32;
            uint64_t u64;
        } epoll_data_t;

        struct epoll_event {
            uint32_t     events;     // Epoll events 
            epoll_data_t data;       // User data variable 
        };

        when epoll_wait returns, this array is modified to indicate 
        information about the subset of file descriptors in the 
        interest list that are in the ready state
    */
    struct epoll_event events[MAX_EVENTS];

    while (ep->running) 
    {
        /*
            - waits for I/O events, blocking the calling 
              thread if no events are currently available.
            - blocks until any of the descriptors being 
              monitored becomes ready for I/O.
            - can unblock if interrupted by a signal
            - can unblock if timeout parameter is not set to -1
            - TODO: set timeout, now its blocking forever
        */
        int nfds = epoll_wait(ep->epoll_fd, events, MAX_EVENTS, 1000);

        if (nfds < 0) 
        {
            if (errno == EINTR) 
            {
                // interrupted by a signal, try again
                continue;
            }
            // serious error occured
            fprintf(stderr, "epoll_wait error: %s\n", strerror(errno));
            break;
        }
        else if (nfds == 0)
        {
            // timed out no idea what to do ??
            // ideas : keep alive sweep periodically
            continue;
        }

        // one or more file descriptors in the interest list became ready
        for (int i = 0; i < nfds; i++) 
        {
            // Get the event context
            event_ctx_t *ctx = (event_ctx_t*)events[i].data.ptr;
            if (!ctx) continue;

            // New connection on listener
            if (ctx->fd == socket_get_socket(&ep->listener)) 
            {
                event_poll_handle_new_connection(ep);
            }
            // Existing connection
            else 
            {
                if (events[i].events & (EPOLLHUP | EPOLLERR)) 
                {
                    event_poll_remove_ctx(ep, ctx);
                    close(ctx->fd);
                    free(ctx);
                } 
                else if (ctx->callback) 
                {
                    uint32_t ev = 0;
                    if (events[i].events & EPOLLIN) ev |= EVENT_READ;
                    if (events[i].events & EPOLLOUT) ev |= EVENT_WRITE;
                    
                    ctx->callback(ctx, ev);
                }
            }
        }
    }
    event_poll_destroy(ep);
}

void event_poll_stop(event_poll_t *ep) 
{
    if (ep) {
        ep->running = false;
    }
}

#endif

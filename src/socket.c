#include "socket.h"
#include <wchar.h>


/* Win32 Specific functions */
#ifdef _WIN32

    socket_handle create_overlapped_socket(int family, int socktype, int protocol)
    {
        /*
            Create socket with WSA_FLAG_OVERLAPPED for IOCP compatibility
            This is required for overlapped I/O operations used by IOCP
        */
        return WSASocketW(family, socktype, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
    }

    typedef struct 
    {
        OVERLAPPED overlapped;
        SOCKET     accept_sock;
        char       buffer[(sizeof(SOCKADDR_IN6)+16)*2];
    } accept_ctx_t;

    LPFN_ACCEPTEX g_AcceptEx = NULL;
    LPFN_GETACCEPTEXSOCKADDRS g_GetAcceptExSockaddrs = NULL;
    
    /*
        The function pointer for the AcceptEx function must be obtained
        at run time by making a call to the WSAIoctl function with the SIO_GET_EXTENSION_FUNCTION_POINTER opcode specified.
        The input buffer passed to the WSAIoctl function must contain WSAID_ACCEPTEX
     */
    int load_winsock_extensions(socket_handle s) 
    {
        if (g_AcceptEx) return 0; // already loaded

        DWORD bytes = 0;
        GUID guidAcceptEx = WSAID_ACCEPTEX;
        GUID guidGetAddr  = WSAID_GETACCEPTEXSOCKADDRS;

        if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     &guidAcceptEx, sizeof(guidAcceptEx),
                     &g_AcceptEx, sizeof(guidAcceptEx),
                     &bytes, NULL, NULL) == SOCKET_ERROR) 
        {
            return -1;
        }

        if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     &guidGetAddr, sizeof(guidGetAddr),
                     &g_GetAcceptExSockaddrs, sizeof(g_GetAcceptExSockaddrs),
                     &bytes, NULL, NULL) == SOCKET_ERROR) 
        {
            g_AcceptEx = NULL;
            return -1;
        }
        return 0;
    }

    /*
        The AcceptEx function combines several socket functions into a single API/kernel transition. 
        The AcceptEx function, when successful, performs three tasks:
            - A new connection is accepted.
            - Both the local and remote addresses for the connection are returned.
            - The first block of data sent by the remote is received.

        The AcceptEx function uses overlapped I/O, unlike the accept function. 
        If your application uses AcceptEx, it can service a large number of clients
        with a relatively small number of threads. As with all overlapped Windows
        functions, either Windows events or completion ports can be used as a 
        completion notification mechanism.

        Another key difference between the AcceptEx function and the accept function
        is that AcceptEx requires the caller to already have two sockets:
            - One that specifies the socket on which to listen.
            - One that specifies the socket on which to accept the connection.
    */
    socket_handle socket_accept_overlapped(Socket *sock, accept_ctx_t* ctx)
    {
        if (!ctx){
            return INVALID_SOCKET;
        }

        memset(ctx, 0, sizeof(*ctx));

        ctx->accept_sock = WSASocketW(AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

        if (ctx->accept_sock == INVALID_SOCKET) {
            return INVALID_SOCKET;
        }

        DWORD bytes = 0;
        BOOL ok = g_AcceptEx(
            sock->sockfd,
            ctx->accept_sock,
            ctx->buffer,
            0,
            sizeof(SOCKADDR_IN6)+16,
            sizeof(SOCKADDR_IN6)+16,
            &bytes,
            &ctx->overlapped
        );

        if (!ok && WSAGetLastError() != WSA_IO_PENDING) 
        {
            closesocket(ctx->accept_sock);
            return INVALID_SOCKET;
        }
        return ctx->accept_sock;
    }
#endif

int socket_init(Socket *sock)
{
    #ifdef _WIN32
        /*
            Before making any other windows sockets calls 
            you must init the windows sockets DLL by calling 
            WSAStartup() and specifying the highest version 
            of the winsock specification used, latest is 2.2
         */
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
            socket_cleanup();
            return -1;
        }

        sock->sockfd = INVALID_SOCKET;
        memset(sock->port, 0, PORT_BUFFER_SIZE);
    #else
        sock->sockfd = -1;
        memset(sock->port, 0, PORT_BUFFER_SIZE);
    #endif

    return 0;
}

void socket_close(Socket *sock)
{
    #ifdef _WIN32
        if (sock->sockfd != INVALID_SOCKET){
            closesocket(sock->sockfd);
            sock->sockfd = INVALID_SOCKET;
        }
    #else
        if (sock->sockfd != -1) {
            close(sock->sockfd);
            sock->sockfd = -1;
        }
    #endif
}

void socket_cleanup(void)
{
    #ifdef _WIN32
        /* 
           WSACleanup terminates Windows Sockets
           operations for all threads. 
        */
        WSACleanup();
    #else
    #endif
}

int socket_tcp_socket(Socket *sock, const char *ip, const char *port)
{
#ifdef _WIN32
   struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(hints));     // Make sure its clean
    hints.ai_family   = AF_UNSPEC;        // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;      // TCP
    hints.ai_flags    = AI_PASSIVE;       // fill in my IP for me

    int err;
    /* Make it protocol independent */
    if ((err = getaddrinfo(ip, port, &hints, &res)) != 0) 
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerrorA(err));
        return -1;
    }

    char last_error[256] = {0};

    /*
        the linked list may have more than one addrinfo structure
        the application should try using the addresses in the order
        in which they are returned until we successfully bind
    */
    for(p = res; p != NULL; p = p->ai_next) 
    {
        // Create a TCP/IP stream socket
        // MSDN say using socket() The socket that is created will have the overlapped attribute as a default.
        // found conflicting info online so I will just be safe
        sock->sockfd = create_overlapped_socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (sock->sockfd == INVALID_SOCKET) 
        {
            snprintf(last_error, sizeof(last_error), "Socket creation failed: %d", WSAGetLastError());
            continue;
        }

        // Non-blocking IO
        socket_set_non_blocking(sock->sockfd);

        // bind the port to the socket
        if (bind(sock->sockfd, p->ai_addr, (int)p->ai_addrlen) == SOCKET_ERROR) 
        {
            snprintf(last_error, sizeof(last_error), "Binding failed: %d", WSAGetLastError());
            socket_close(sock);
            continue;
        }
        break;
    }    

    // not needed anymore
    freeaddrinfo(res);

    // didnt bind
    if(p == NULL){
        fprintf(stderr, "Failed to bind to any address. Last error: %s\n", last_error);
        return -1;
    }

    strncpy_s(sock->port, PORT_BUFFER_SIZE, port, _TRUNCATE);   

    return 0;
#else
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(hints));     // Make sure its clean
    hints.ai_family   = AF_UNSPEC;        // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;      // TCP
    hints.ai_flags    = AI_PASSIVE;       // fill in my IP for me

    int err;
    /* Make it protocol independent */
    if ((err = getaddrinfo(ip, port, &hints, &res)) < 0) 
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(err));
        return;
    }

    char last_error[256] = {0};

    /*
        the linked list may have more than one addrinfo structure
        the application should try using the addresses in the order
        in which they are returned until we successfully bind
    */
    for(p = res; p != NULL; p = p->ai_next) 
    {
        // Create a TCP/IP stream socket
        if ((sock->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
        {
            snprintf(last_error, sizeof(last_error), "Socket creation failed: %s", strerror(errno));
            continue;
        }

        // Non-blocking IO
        socket_set_non_blocking(sock->sockfd);

        // Prevent the "Address already in use" error message
        socket_set_opt_reuse_addr(sock->sockfd, 1);

        // bind the port to the socket
        if (bind(sock->sockfd, p->ai_addr, p->ai_addrlen) < 0) 
        {
            snprintf(last_error, sizeof(last_error), "Binding failed: %s", strerror(errno));
            socket_close(sock);
            continue;
        }
        break;
    }    

    // not needed anymore
    freeaddrinfo(res);

    // didnt bind
    if(p == NULL)
    {
        fprintf(stderr, "Failed to bind to any address. Last error: %s\n", last_error);
        return;
    }

    strncpy(sock->port, port, PORT_BUFFER_SIZE - 1);
#endif
}

int socket_listen_connection(Socket *sock)
{
#ifdef _WIN32
    /* Convert socket to listening socket */
    if (listen(sock->sockfd, BACKLOG) == SOCKET_ERROR)
    {          
        closesocket(sock->sockfd);
        fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
        return -1;
    }

    if(load_winsock_extensions(sock->sockfd) < 0)
    {
        fprintf(stderr, "WSAIoctl failed with error: %u\n", WSAGetLastError());
        closesocket(sock->sockfd);
        return -1;
    }    

    char hostname[HOSTNAME_BUFFER_SIZE];
    char ipaddr[INET6_ADDRSTRLEN];
    
    socket_get_host_name(hostname, sizeof(hostname));
    socket_get_host_ip_addr(ipaddr, sizeof(ipaddr));
    
    printf("Server listening on %s http://%s:%s\n", hostname, ipaddr, sock->port);
#else
    /* Convert socket to listening socket */
    if (listen(sock->sockfd, BACKLOG) < 0) 
    {          
        close(sock->sockfd);
        fprintf(stderr, "Listen failed: %s\n", strerror(errno));
        return;
    }

    char hostname[HOSTNAME_BUFFER_SIZE];
    char ipaddr[INET6_ADDRSTRLEN];
    
    socket_get_host_name(hostname, sizeof(hostname));
    socket_get_host_ip_addr(ipaddr, sizeof(ipaddr));
    
    printf("Server listening on %s http://%s:%s\n", hostname, ipaddr, sock->port);
#endif
    return 0;
}

socket_handle socket_accept_connection(Socket *sock)
{
#ifdef _WIN32
    socket_handle connfd;
    struct sockaddr_storage client_addr;
    int cl_addr_len = sizeof(client_addr);

    /*  A new descriptor is returned by accept for each client that connects to the server. */
    connfd = accept(sock->sockfd, (struct sockaddr *)&client_addr, &cl_addr_len);

    if (connfd == INVALID_SOCKET) 
    {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK || error == WSAEINTR) 
        {
            // normal accept doesnt block
            return INVALID_SOCKET;
        }
        else
        {
            // actual error occured
            fprintf(stderr, "Accept error: %d\n", error);
            return INVALID_SOCKET;
        }
    }

    // char client_ip[INET6_ADDRSTRLEN];
    // socket_get_ip_addr((struct sockaddr *)&client_addr, client_ip, sizeof(client_ip));
    // printf("Client connected from IP: %s\n", client_ip);

    return connfd;
#else
    int connfd;
    struct sockaddr_storage client_addr;
    socklen_t cl_addr_len = sizeof(client_addr);

    /*  A new descriptor is returned by accept for each client that connects to the server. */
    if ((connfd = accept(sock->sockfd, (struct sockaddr *)&client_addr, &cl_addr_len)) < 0) 
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "Accept error : %s\n", strerror(errno));
        }
        return -1;
    }

    // char client_ip[INET6_ADDRSTRLEN];
    // socket_get_ip_addr((struct sockaddr *)&client_addr, client_ip, sizeof(client_ip));
    // printf("Client connected from IP: %s\n", client_ip);

    return connfd;
#endif
}

socket_handle socket_connect(const char *ip, const char *port) 
{
#ifdef _WIN32
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(ip, port, &hints, &res) != 0) 
    {
        fprintf(stderr, "Failed to resolve address\n");
        return INVALID_SOCKET;
    }
    
    socket_handle sockfd = create_overlapped_socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd == INVALID_SOCKET) 
    {
        fprintf(stderr, "Failed to create socket: %d\n", WSAGetLastError());
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }
    
    if (connect(sockfd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) 
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) 
        {
            fprintf(stderr, "Failed to connect to server: %d\n", error);
            closesocket(sockfd);
            freeaddrinfo(res);
            return INVALID_SOCKET;
        }
    }
    
    freeaddrinfo(res);
    return sockfd;
#else
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(ip, port, &hints, &res) != 0) 
    {
        fprintf(stderr, "Failed to resolve address\n");
        return -1;
    }
    
    socket_handle sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        freeaddrinfo(res);
        return -1;
    }
    
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) 
    {
        fprintf(stderr, "Failed to connect to server\n");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);
    return sockfd;
#endif
}

socket_handle socket_get_handle(const Socket *sock)
{
    return sock->sockfd;
}

char* socket_get_host_ip_addr(char *buffer, size_t bufsize)
{
#ifdef _WIN32
    /* https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses?redirectedfrom=MSDN */
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    ULONG outBufLen = 15000;
    DWORD dwRetVal = 0;
    ULONG family = AF_UNSPEC;
    
    strncpy_s(buffer, bufsize, "No IP address found", _TRUNCATE);

    // Allocate memory for adapter addresses
    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
    if (pAddresses == NULL) 
    {
        strncpy_s(buffer, bufsize, "Memory allocation failed", _TRUNCATE);
        return buffer;
    }

    // Get adapter addresses
    dwRetVal = GetAdaptersAddresses(family, 
                                    GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                    NULL, pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) 
    {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        if (pAddresses == NULL) 
        {
            strncpy_s(buffer, bufsize, "Memory allocation failed", _TRUNCATE);
            return buffer;
        }
        dwRetVal = GetAdaptersAddresses(AF_UNSPEC,
                                        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                        NULL, pAddresses, &outBufLen);
    }

    if (dwRetVal == NO_ERROR) 
    {
        // If successful, iterate the linked list
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) 
        {
            if ((pCurrAddresses->IfType == IF_TYPE_ETHERNET_CSMACD ||
                pCurrAddresses->IfType == IF_TYPE_IEEE80211) &&  
                pCurrAddresses->OperStatus == IfOperStatusUp)
            {
                BOOL isVirtual = FALSE;
                if (pCurrAddresses->Description) {
                    // Check for common virtual adapter keywords
                    if (wcsstr(pCurrAddresses->Description, L"Virtual") ||
                        wcsstr(pCurrAddresses->Description, L"Hyper-V") ||
                        wcsstr(pCurrAddresses->Description, L"VMware") ||
                        wcsstr(pCurrAddresses->Description, L"VirtualBox") ||
                        wcsstr(pCurrAddresses->Description, L"WSL") ||
                        wcsstr(pCurrAddresses->Description, L"vEthernet")) {
                        isVirtual = TRUE;
                    }
                }
                if(!isVirtual)
                {
                    // get unicast addresses
                    pUnicast = pCurrAddresses->FirstUnicastAddress;

                    while (pUnicast != NULL) 
                    {
                        /* Protocol Independent */
                        if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) 
                        {
                            struct sockaddr_in *sa_in = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                            char addressBuffer[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, &(sa_in->sin_addr), addressBuffer, INET_ADDRSTRLEN);
                            
                            // Skip the loopback address and take the first non-loopback IPv4 address
                            if (strcmp(addressBuffer, "127.0.0.1") != 0 &&
                                strncmp(addressBuffer, "169.254.", 8) != 0) 
                            {
                                strncpy_s(buffer, bufsize, addressBuffer, _TRUNCATE);
                                goto cleanup;
                            }
                        }
                        else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) 
                        {
                            struct sockaddr_in6 *sa_in6 = (struct sockaddr_in6 *)pUnicast->Address.lpSockaddr;
                            char addressBuffer[INET6_ADDRSTRLEN];
                            inet_ntop(AF_INET6, &(sa_in6->sin6_addr), addressBuffer, INET6_ADDRSTRLEN);
                            
                            // Skip loopback address and link-local addresses
                            if (strncmp(addressBuffer, "::1", 3) != 0 && 
                                strncmp(addressBuffer, "fe80:", 5) != 0) {
                                strncpy_s(buffer, bufsize, addressBuffer, _TRUNCATE);
                            }
                        }
                        pUnicast = pUnicast->Next;
                    }
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    } 
    else 
    {
        fprintf(stderr, "GetAdaptersAddresses failed with error: %d\n", dwRetVal);
        strncpy_s(buffer, bufsize, "Error getting network interfaces", _TRUNCATE);
    }

cleanup:
    if (pAddresses) {
        free(pAddresses);
    }

    return buffer;
#else
    struct ifaddrs *ifAddrStruct = NULL;
    struct ifaddrs *ifa = NULL;
    void *tmpAddrPtr = NULL;
    
    strncpy(buffer, "No IP address found", bufsize - 1);
    buffer[bufsize - 1] = '\0';

    if (getifaddrs(&ifAddrStruct) == -1) 
    {
        perror("getifaddrs");
        strncpy(buffer, "Error getting network interfaces", bufsize - 1);
        return buffer;
    }

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) 
    {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        /* Protocol Independent */
        if (ifa->ifa_addr->sa_family == AF_INET) 
        {
            tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

            // Skip the loopback address and take the first non-loopback IPv4 address
            if (strcmp(addressBuffer, "127.0.0.1") != 0) 
            {
                strncpy(buffer, addressBuffer, bufsize - 1);
                buffer[bufsize - 1] = '\0';
                break;
            }
        }
        else if (ifa->ifa_addr->sa_family == AF_INET6) 
        {
            tmpAddrPtr = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            
            // Skip loopback address and link-local addresses
            if (strncmp(addressBuffer, "::1", 3) != 0 &&
                strncmp(addressBuffer, "fe80:", 5) != 0) 
            {
                strncpy(buffer, addressBuffer, bufsize - 1);
                buffer[bufsize - 1] = '\0';
            }
        }
    }

    if (ifAddrStruct != NULL) {
        freeifaddrs(ifAddrStruct);
    }

    return buffer;
#endif
}

char* socket_get_host_name(char *buffer, size_t bufsize)
{
#ifdef _WIN32
    if (gethostname(buffer, (int)bufsize) == SOCKET_ERROR) 
    {
        fprintf(stderr, "gethostname error: %d\n", WSAGetLastError());
        strncpy_s(buffer, bufsize, "Error getting hostname", _TRUNCATE);
    }
    
    return buffer;
#else
    if (gethostname(buffer, bufsize) == -1) 
    {
        perror("get_host_name");
        strncpy(buffer, "Error getting hostname", bufsize - 1);
        buffer[bufsize - 1] = '\0';
    }
    
    return buffer;
#endif
}

char* socket_get_ip_addr(struct sockaddr *sa, char *buffer, size_t bufsize)
{
#ifdef _WIN32
    void *addr = NULL;

    /* Protocol Independent */
    switch (sa->sa_family) 
    {
        case AF_INET: { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)sa;
            addr = &(ipv4->sin_addr);
            break;
        }
        case AF_INET6: { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)sa;
            addr = &(ipv6->sin6_addr);
            break;
        }
        default:
            strncpy_s(buffer, bufsize, "Unknown AF", _TRUNCATE);
            return buffer; // Address family not handled
    }
    // Convert address to a string and store it in buffer
    inet_ntop(sa->sa_family, addr, buffer, bufsize);
    return buffer;
#else
    void *addr = NULL;

    /* Protocol Independent */
    switch (sa->sa_family) 
    {
        case AF_INET: { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)sa;
            addr = &(ipv4->sin_addr);
            break;
        }
        case AF_INET6: { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)sa;
            addr = &(ipv6->sin6_addr);
            break;
        }
        default:
            strncpy(buffer, "Unknown AF", bufsize - 1);
            buffer[bufsize - 1] = '\0';
            return buffer; // Address family not handled
    }
    // Convert address to a string and store it in buffer
    inet_ntop(sa->sa_family, addr, buffer, bufsize);
    return buffer;
#endif
}

typedef struct {
    uint32_t ip;
    uint16_t port;
} SocketAddress;

/* UDP */
int socket_send_to(socket_handle sockfd, const SocketAddress *address, 
                   const void *data, int length) 
{
    if (sockfd < 0 || length == 0 || data == NULL || address == NULL) {
        return 0;
    }

    struct sockaddr_in addr_in;
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(address->port);
    addr_in.sin_addr.s_addr = htonl(address->ip);

    int ret = sendto(sockfd, data, length, 0, 
                    (struct sockaddr *)&addr_in, sizeof(addr_in));

    if (ret < 0) {
#ifdef _WIN32
        fprintf(stderr, "sendto() failed: %d\n", WSAGetLastError());
#else
        fprintf(stderr, "sendto() failed: %s\n", strerror(errno));
#endif
    }

    return ret;
}

/* UDP */
int socket_recv_from(socket_handle sockfd, SocketAddress *address, void *data, int length) 
{
    if (sockfd < 0 || data == NULL) {
        return 0;
    }

    struct sockaddr_in addr_in;
    socklen_t addr_len = sizeof(addr_in);
    
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = INADDR_ANY;

    int len = recvfrom(sockfd, data, length, 0, 
                      (struct sockaddr *)&addr_in, &addr_len);

    if (len > 0 && address != NULL) {
        address->ip = ntohl(addr_in.sin_addr.s_addr);
        address->port = ntohs(addr_in.sin_port);
    }

    if (len < 0) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            fprintf(stderr, "recvfrom() failed: %d\n", error);
        }
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "recvfrom() failed: %s\n", strerror(errno));
        }
#endif
    }

    return len;
}

int socket_send(socket_handle sockfd, const void *data, size_t length) 
{
    if (sockfd < 0 || length == 0 || data == NULL) {
        return 0;
    }

    int ret = send(sockfd, data, length, 0);

    if (ret < 0) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            fprintf(stderr, "send() failed: %d\n", error);
        }
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "send() failed: %s\n", strerror(errno));
        }
#endif
    }

    return ret;
}

int socket_recv(socket_handle sockfd, void *data, int length) 
{
    if (sockfd < 0 || data == NULL) {
        return 0;
    }

    int len = recv(sockfd, data, length, 0);

    if (len < 0) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            fprintf(stderr, "recv() failed: %d\n", error);
        }
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "recv() failed: %s\n", strerror(errno));
        }
#endif
    }

    return len;
}

void socket_set_non_blocking(socket_handle fd)
{
#ifdef _WIN32
    /* 
        - set socket for nonblocking I/O
        To put a socket into nonblocking mode, pass cmd as the constant FIONBIO
        and point argp or lpInBuffer to a non-zero variable. 
        - I/O system calls that would block now will return SOCKET_ERROR
          and WSAGetLastError() will return WSAEWOULDBLOCK
    */
    u_long mode = 1;  // 1 to enable non-blocking mode
    if (ioctlsocket(fd, FIONBIO, &mode) == SOCKET_ERROR) {
        fprintf(stderr, "ioctlsocket failed: %d\n", WSAGetLastError());
    }
#else
    /* 
        - file control : set socket for nonblocking I/O
        - Beware not to clear all the other file status flags.  
        - I/O system calls that would block now will return -1
          and errno will be set to EWOULDBLOCK or EAGAIN
    */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags|O_NONBLOCK);
#endif
}

void socket_set_opt_reuse_addr(socket_handle sockfd, int on)
{
#ifdef _WIN32
    /*
    * Not the same behaviour on windows 
    * Once the second socket has successfully bound, the behavior for all sockets bound to that port is indeterminate.
    * For example, if all of the sockets on the same port provide TCP service, any incoming TCP connection requests 
    * over the port cannot be guaranteed to be handled by the correct socket — the behavior is non-deterministic
    */
    BOOL optval = on ? TRUE : FALSE;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR) {
        fprintf(stderr, "setsockopt error: socket_set_opt_reuse_addr %d\n", WSAGetLastError());
    }
#else
    // Prevent the "Address already in use" error message
    int optval = on ? 1 : 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        fprintf(stderr, "setsockopt error : socket_set_opt_reuse_addr %s\n", strerror(errno));
    }
#endif
}

void socket_set_opt_keep_alive(socket_handle sockfd, int on)
{
#ifdef _WIN32
    BOOL optval = on ? TRUE : FALSE;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR) {
        fprintf(stderr, "setsockopt error: socket_set_opt_keep_alive %d\n", WSAGetLastError());
    }    
#else
    int optval = on ? 1 : 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
        fprintf(stderr, "setsockopt error : socket_set_opt_keep_alive %s\n", strerror(errno));
    }
#endif

}

// Disable Nagle's Algorithm
void socket_set_opt_tcp_no_delay(socket_handle sockfd, int on)
{
#ifdef _WIN32
    BOOL optval = on ? TRUE : FALSE;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR) {
        fprintf(stderr, "setsockopt error: socket_set_opt_tcp_no_delay %d\n", WSAGetLastError());
    }
#else
    int optval = on ? 1 : 0;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
        fprintf(stderr, "setsockopt error : socket_set_opt_tcp_no_delay %s\n", strerror(errno));
    }
    #endif
}

// allows data to be sent in the SYN packet, speeding up 
// connection setup for subsequent connections to the same server.
void socket_set_opt_tcp_fast_open(socket_handle sockfd, int qlen)
{
#ifdef _WIN32
   // Note: TCP_FASTOPEN is available on Windows 10 version 1607 and later
    // The constant might not be defined in older SDKs
    #ifdef TCP_FASTOPEN
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_FASTOPEN, (const char*)&qlen, sizeof(qlen)) == SOCKET_ERROR) {
        fprintf(stderr, "setsockopt error: socket_set_opt_tcp_fast_open %d\n", WSAGetLastError());
    }
    #else
    fprintf(stderr, "TCP_FASTOPEN not supported on this platform\n");
    #endif
#else
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen)) < 0) {
        fprintf(stderr, "setsockopt error : socket_set_opt_tcp_fast_open %s\n", strerror(errno));
    }
#endif
}    

void socket_set_opt_tcp_quick_ack(socket_handle sockfd, int on)
{
#ifdef _WIN32
    (void)sockfd;
    (void)on;
#else
    int quickack = on ? 1 : 0;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_QUICKACK, &quickack, sizeof(quickack)) < 0) {
        fprintf(stderr, "setsockopt error : socket_set_opt_tcp_quick_ack %s\n", strerror(errno));
    }
#endif
}

void socket_set_opt_linger(socket_handle sockfd)
{
#ifdef _WIN32
    struct linger linger_opt = {1, 10};  // Enable linger with a 10-second timeout
    if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char*)&linger_opt, sizeof(linger_opt)) == SOCKET_ERROR) {
        fprintf(stderr, "setsockopt error: socket_set_opt_linger %d\n", WSAGetLastError());
    }
#else
    struct linger linger_opt = {1, 10};  // Enable linger with a 10-second timeout
    if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) < 0) {
        fprintf(stderr, "setsockopt error : socket_set_opt_linger %s\n", strerror(errno));
    }
#endif
}

void socket_set_opt_rcvbuf(socket_handle sockfd, int bufsize)
{
#ifdef _WIN32
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&bufsize, sizeof(bufsize)) == SOCKET_ERROR) {
        fprintf(stderr, "setsockopt error: socket_set_opt_rcvbuf %d\n", WSAGetLastError());
    }
#else
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
        fprintf(stderr, "setsockopt error : socket_set_opt_rcvbuf %s\n", strerror(errno));
    }
#endif

}

void socket_set_opt_sndbuf(socket_handle sockfd, int bufsize)
{
#ifdef _WIN32
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)&bufsize, sizeof(bufsize)) == SOCKET_ERROR) {
        fprintf(stderr, "setsockopt error: socket_set_opt_sndbuf %d\n", WSAGetLastError());
    }
#else
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0) {
        fprintf(stderr, "setsockopt error : socket_set_opt_sndbuf %s\n", strerror(errno));
    }
#endif

}

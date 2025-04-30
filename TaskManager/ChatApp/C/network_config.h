#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <netinet/tcp.h>
#include <sys/socket.h>

// TCP buffer sizes
#define TCP_SEND_BUFFER_SIZE (256 * 1024)  // 256KB
#define TCP_RECV_BUFFER_SIZE (256 * 1024)  // 256KB

// TCP keepalive settings
#define TCP_KEEPALIVE_TIME 60     // Time in seconds before sending keepalive probes
#define TCP_KEEPALIVE_INTVL 10    // Time in seconds between keepalive probes
#define TCP_KEEPALIVE_PROBES 6    // Number of keepalive probes before connection is considered dead

// Connection pool settings
#define CONN_POOL_INITIAL_SIZE 10
#define CONN_POOL_MAX_SIZE 1000
#define CONN_POOL_GROW_SIZE 10

// Thread pool settings
#define THREAD_POOL_MIN_SIZE 4
#define THREAD_POOL_MAX_SIZE 32
#define THREAD_POOL_QUEUE_SIZE 1000

// Rate limiting settings
#define RATE_LIMIT_MESSAGES 30    // Maximum messages per minute
#define RATE_LIMIT_CONNECTIONS 5  // Maximum connection attempts per minute

// Socket timeout values (in seconds)
#define SOCKET_CONNECT_TIMEOUT 30
#define SOCKET_READ_TIMEOUT 300
#define SOCKET_WRITE_TIMEOUT 60

// Configure TCP socket with optimized settings
static int configure_tcp_socket(int sockfd) {
    int opt;
    
    // Enable TCP keepalive
    opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        return -1;
    }
    
#ifdef __APPLE__
    // macOS specific keepalive settings
    opt = TCP_KEEPALIVE_TIME;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        return -1;
    }
#else
    // Linux specific keepalive settings
    opt = TCP_KEEPALIVE_TIME;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(opt)) < 0) {
        return -1;
    }
    
    opt = TCP_KEEPALIVE_INTVL;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt)) < 0) {
        return -1;
    }
    
    opt = TCP_KEEPALIVE_PROBES;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(opt)) < 0) {
        return -1;
    }
#endif
    
    // Set TCP buffer sizes
    opt = TCP_SEND_BUFFER_SIZE;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt)) < 0) {
        return -1;
    }
    
    opt = TCP_RECV_BUFFER_SIZE;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt)) < 0) {
        return -1;
    }
    
    // Enable TCP_NODELAY (disable Nagle's algorithm)
    opt = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        return -1;
    }
    
    // Enable address reuse
    opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        return -1;
    }
    
    return 0;
}

#endif // NETWORK_CONFIG_H 
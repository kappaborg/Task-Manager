#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

// Network configuration
#define MAX_CLIENTS 50
#define MAX_ROOMS 20
#define MAX_USERS_PER_ROOM 20
#define MAX_MESSAGE_SIZE 4096
#define MAX_USERNAME_LENGTH 32
#define MAX_ROOM_NAME_LENGTH 64
#define KEEPALIVE_TIME 60
#define KEEPALIVE_INTERVAL 15
#define KEEPALIVE_PROBES 4

// Message types
typedef enum {
    MSG_CHAT = 1,
    MSG_PRIVATE,
    MSG_ROOM,
    MSG_STATUS,
    MSG_FILE,
    MSG_ADMIN,
    MSG_ERROR,
    MSG_INFO
} message_type_t;

// User status
typedef enum {
    STATUS_OFFLINE = 0,
    STATUS_ONLINE,
    STATUS_AWAY,
    STATUS_BUSY
} user_status_t;

// Room type
typedef enum {
    ROOM_PUBLIC = 1,
    ROOM_PRIVATE,
    ROOM_MODERATED
} room_type_t;

// Configure TCP socket for optimal chat performance
static inline int configure_tcp_socket(int sockfd) {
    int flag = 1;
    int keepalive_time = KEEPALIVE_TIME;
    
    // Disable Nagle's algorithm
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        return -1;
    }
    
    // Enable keep-alive
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0) {
        return -1;
    }
    
#ifdef __APPLE__
    // macOS specific keep-alive settings
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPALIVE, &keepalive_time, sizeof(int)) < 0) {
        return -1;
    }
#else
    // Linux specific keep-alive settings
    int keepalive_interval = KEEPALIVE_INTERVAL;
    int keepalive_probes = KEEPALIVE_PROBES;
    
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_time, sizeof(int)) < 0) {
        return -1;
    }
    
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_interval, sizeof(int)) < 0) {
        return -1;
    }
    
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_probes, sizeof(int)) < 0) {
        return -1;
    }
#endif
    
    // Set non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    
    return 0;
}

#endif // NETWORK_CONFIG_H 
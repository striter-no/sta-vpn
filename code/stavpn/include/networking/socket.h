#ifndef STA_NSOCK
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef struct {
    char           ip[24];
    unsigned short port;

    int                fd;
    socklen_t          len;
    struct sockaddr_in addr;
} sta_sock;

int sta_sock_qsetopt(sta_sock sock, int OPT_NAME, int value){
    return setsockopt(sock.fd, IPPROTO_TCP, OPT_NAME, &value, sizeof(value));
}

void sta_sock_close(sta_sock sock){
    close(sock.fd);
}

int sta_sock_resolution(const char *domain){
    
}

#define NULL_SOCKET (sta_sock){"", 0, 0, 0, (struct sockaddr_in){0}}

#endif
#define STA_NSOCK

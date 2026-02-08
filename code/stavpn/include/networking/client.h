#include "socket.h"
#include "stream.h"
#include "string.h"
#include <netinet/in.h>
#include <sys/socket.h>

#ifndef STA_NCLI

typedef struct {
    sta_sock socket;
} sta_ncli;

// * creating socket
int sta_ncli_create(
    const char *server_ip,
    unsigned int server_port,

    sta_ncli *out
){
    sta_sock sock;
    strcpy(sock.ip, server_ip);
    sock.port = server_port;
    
    sock.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock.fd <= 0) return -1;
    
    sock.addr = (struct sockaddr_in){
        .sin_addr = {inet_addr(server_ip)},
        .sin_family = AF_INET,
        .sin_port = htons(server_port),
        .sin_zero = {0}
    };

    sock.len = sizeof(sock.addr);

    *out = (sta_ncli){sock};
    return 0;
}

// * blocking `connect()`
int sta_ncli_connect(sta_ncli cli){
    if (cli.socket.fd <= 0) return -1;

    if ((connect(
        cli.socket.fd, 
        (const struct sockaddr*)&cli.socket.addr, 
        cli.socket.len
    )) < 0){
        return -1;
    }

    return 0;
}

// * closing internal socket
int sta_ncli_disconnect(sta_ncli cli){
    if (cli.socket.fd <= 0) return -1;
    close(cli.socket.fd);
    return 0;
}

// * creating stream for client
sta_stream sta_ncli_stream(sta_ncli *cli){
    return sta_stream_create(&cli->socket);
}

#endif
#define STA_NCLI
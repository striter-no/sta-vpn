#include "socket.h"
#include "stream.h"
#include "string.h"
#include <netinet/in.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <stdbool.h>

#ifndef STA_NSERV

typedef struct {
    sta_sock sock;

    size_t listen_to;
    size_t active_clis;
    atomic_bool is_active;
} sta_nserv;

// * creating socket
int sta_nserv_create(
    const char *bind_ip,
    unsigned int bind_port,
    size_t max_active_clis,

    sta_nserv *out
){
    sta_sock sock;
    sock.addr = (struct sockaddr_in){
        .sin_addr = {inet_addr(bind_ip)},
        .sin_port = htons(bind_port),
        .sin_zero = {0},
        .sin_family = AF_INET
    };

    strcpy(sock.ip, bind_ip);
    sock.port = bind_port;
    sock.len = sizeof(sock.addr);

    sock.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock.fd <= 0){
        return -1;
    }

    *out = (sta_nserv){
        .sock = sock,
        .listen_to = max_active_clis,
        .active_clis = 0
    };
    atomic_store(&out->is_active, true);
    return 0;
}

// * binding to given ip
// * auto-listening for N clients
int sta_nserv_bindnlisten(sta_nserv serv){
    if (serv.sock.fd <= 0 || bind(
        serv.sock.fd, 
        (const struct sockaddr*)&serv.sock.addr, 
        serv.sock.len)
    ) return -1;

    if (serv.listen_to == 0) return -2;
    if (listen(
        serv.sock.fd, 
        serv.listen_to
    ) < 0) return -1;

    return 0;
}

// * stopping all client threads
// * closing socket
int sta_nserv_stop(sta_nserv *serv){
    atomic_store(&serv->is_active, false);
    close(serv->sock.fd);
    return 0;
}

// * blocking `accept()`
// * accepting new client as `sta_sock`
int sta_nserv_accept(sta_nserv serv, sta_sock *output){
    struct sockaddr_in addr;
    socklen_t          length = sizeof(addr);

    int cli_fd = accept(serv.sock.fd, (struct sockaddr *)&addr, &length);
    if (cli_fd <= 0) return -1;

    *output = (sta_sock){
        .fd = cli_fd,
        .addr = addr,
        .len = length,
        .port = ntohs(addr.sin_port)
    };
    strcpy(output->ip, inet_ntoa(addr.sin_addr));
    return 0;
}

#endif
#define STA_NSERV
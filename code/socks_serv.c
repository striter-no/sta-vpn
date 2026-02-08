#include "networking/socket.h"
#include <socks5/server.h>
#include <stdio.h>

void connection_handler(
    sta_sock client_socket, 
    sta_addr remote_address, 
    sta_sock *out_remote,
    void *state_holder
){
    if (0 > sta_resolve_address(remote_address, out_remote)){
        out_remote->fd = 0;
    }
}

int tunnel_iter(
    sta_sock client_socket, 
    sta_sock remote_socket, 
    int who,
    void *state_holder
){
    char buffer[BUFSIZ] = {0};
    sta_sock sockets[2] = {client_socket, remote_socket};

    ssize_t len = read(sockets[who].fd, buffer, BUFSIZ);
    if (len <= 0) return -1;
    write(sockets[1 - who].fd, buffer, BUFSIZ);

    return 0;
}

int main(void){
    const char     ip[] = "127.0.0.1";
    unsigned short port = 9000;

    sta_socks5_proxy proxy;
    if (0 > sta_socks5_create(ip, port, 20, &proxy)) {
        perror("sta_socks5_create");
        return -1;
    }

    sta_sock_qsetopt(proxy.proxy_server.sock, SO_REUSEPORT, 1);
    if (0 > sta_socks5_bindnlisten(proxy)){
        perror("sta_socks5_bindnlisten");
        return -1;
    }
    sta_socks5_accept_loop(
        &proxy, 
        tunnel_iter, 
        connection_handler,
        NULL
    );

    sta_socks5_pclose(&proxy);
}
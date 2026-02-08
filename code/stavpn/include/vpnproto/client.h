#include <bits/pthreadtypes.h>
#include <socks5/server.h>
#include <networking/socket.h>
#include <pthread.h>

#ifndef STA_VPNCLIENT

typedef struct {
    sta_socks5_proxy proxy;
    char             vpn_ip[24];
    unsigned short   vpn_port;

    sta_ncli vpn_client;
    pthread_mutex_t stream_mtx;
} sta_vpnclient;

// * initializes socks5 proxy
// * binds to the interface socks5 proxy
// * starts to listen
int sta_vpn_client_init(
    const char *socks5_ip,
    unsigned short socks5_port,
    size_t max_clients,

    const char *vpn_ip,
    unsigned short vpn_port,

    sta_vpnclient *output
){
    strcpy(output->vpn_ip, vpn_ip);
    output->vpn_port = vpn_port;

    if (0 > sta_socks5_create(
        socks5_ip, socks5_port, 
        max_clients, &output->proxy
    )){
        perror("sta_socks5_create");
        return -1;
    }

    sta_sock_qsetopt(output->proxy.proxy_server.sock, SO_REUSEPORT, 1);
    if (0 > sta_socks5_bindnlisten(output->proxy)){
        perror("sta_socks5_bindnlisten");
        return -1;
    }

    return 0;
}

// * client_socket - origin, who initiated connection (doesnt matter)
// * remote_address - requested address
// * state_holder - vpn server
// * function requests connection to vpn server and registers a new pair 
//   for client <-> remote
void __sta_vpn_cli_conn_handler(
    sta_sock client_socket, 
    sta_addr remote_address, 
    sta_sock *out_remote,
    void *_state_holder
){
    sta_vpnclient *vcl = _state_holder;
    pthread_mutex_lock(&vcl->stream_mtx);

    // 2. ask for registration (new stream) at vpn
    // 3. in out_remote write vpn socket

    pthread_mutex_unlock(&vcl->stream_mtx);
}

// * forwards data from local client to remote (vpn)
// * remote_socket is VPN
// * state_holder is also a VPN
// * data to forward is in client_socket
int  __sta_vpn_cli_tunnel_iter(
    sta_sock client_socket, 
    sta_sock remote_socket, 
    int who,
    void *_state_holder
){
    sta_vpnclient *vcl = _state_holder;

    pthread_mutex_lock(&vcl->stream_mtx);
    // 1. send data to vpn
    // 2. recv data from vpn
    pthread_mutex_unlock(&vcl->stream_mtx);
    return 0;
}

// * starts main accept loop (socks5)
// * does not connect to VPN, because connection will be performed in R/W cycle
int sta_vpn_client_run(sta_vpnclient *client){
    pthread_mutex_init(&client->stream_mtx, NULL);

    if (0 > sta_ncli_create(client->vpn_ip, client->vpn_port, &client->vpn_client)){
        perror("sta_ncli_create()");
        return -1;
    }

    if (0 > sta_ncli_connect(client->vpn_client)){
        perror("sta_ncli_connect()");
        return -1;
    }

    return sta_socks5_accept_loop(
        &client->proxy, 
        __sta_vpn_cli_tunnel_iter, 
        __sta_vpn_cli_conn_handler,
        client
    );
}

void sta_vpn_client_stop(sta_vpnclient *client){
    sta_socks5_pclose(&client->proxy);
    sta_ncli_disconnect(client->vpn_client);
    pthread_mutex_destroy(&client->stream_mtx);
}

#endif
#define STA_VPNCLIENT
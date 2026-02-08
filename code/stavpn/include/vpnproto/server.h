#include "networking/socket.h"
#include <pthread.h>
#include <socks5/server.h>
#include <qol/kvtable.h>

#ifndef STA_VPNSERVER

typedef struct {
    sta_nserv      lserver;
    char           bind_ip[24];
    unsigned short bind_port;

    // TODO: Test safety of the `fd` key
    // kvtable: client's fd -> remote ip:port
    // unordered_kvtable clients;
} sta_vpnserver;

int sta_vpn_create(
    const char    *ip,
    unsigned short port,
    size_t max_clients,

    sta_vpnserver *output
){
    output->bind_port = port;
    strcpy(output->bind_ip, ip);

    if (0 > sta_nserv_create(ip, port, max_clients, &output->lserver)){
        return -1;
    }

    // output->clients = kvtable_create(sizeof(int), sizeof(ip_address));
    return 0;
}

int sta_vpn_bindnlisten(sta_vpnserver serv){
    return sta_nserv_bindnlisten(serv.lserver);
}

struct __sta_vpn_worker_args {
    sta_vpnserver *server;
    sta_sock       client;
};

void *__sta_vpn_worker(void *_args){
    struct __sta_vpn_worker_args *args = _args;
    sta_vpnserver *server = args->server;
    sta_sock   client     = args->client;
    sta_stream cli_stream = sta_stream_create(&client);

    // firstly read connection destination
    sta_addr address;
    if (0 > stream_read_exact(cli_stream, sizeof(address), &address)){
        free(args);
        sta_sock_close(client);
        return NULL;        
    }

    sta_sock remote_sock = {0};
    if (0 > sta_resolve_address(address, &remote_sock)){
        goto err;
    }

    struct pollfd fds[2] = {
        {.fd = client.fd,      .events = POLLIN},
        {.fd = remote_sock.fd, .events = POLLIN}
    };

    while (atomic_load(&server->lserver.is_active)){
        int ret = poll(fds, 2, 100);
        if (ret == -1) {
            perror("vpn_worker:poll()");
            break;
        }
        
        if (ret == 0) continue;
        printf("polling, has new events...\n");
        for (size_t i = 0; i < 2; i++){
            if (fds[i].revents & POLLIN){
                char buffer[BUFSIZ] = {0};
                ssize_t len = read(fds[i].fd, buffer, BUFSIZ);
                if (len <= 0) goto tunbreak;
                write(fds[1 - i].fd, buffer, BUFSIZ);
            } else if (fds[i].revents & (POLLERR | POLLHUP)){
                printf("POLLERR | POLLHUP\n");
                goto tunbreak;
            }
        }

        continue;
        tunbreak: break;
    }

err:
    sta_sock_close(remote_sock);
    sta_sock_close(client);
    free(args);
    return NULL;
}

int sta_vpn_loop(sta_vpnserver *serv){
    sta_nserv *server = &serv->lserver;

    while (atomic_load(&server->is_active)){
        sta_sock client;
        
        if (0 > sta_nserv_accept(*server, &client)){
            perror("sta_nserv_accept");
            return -1;
        }

        struct __sta_vpn_worker_args *args = malloc(sizeof(struct __sta_vpn_worker_args));
        args->client = client;
        args->server = serv;

        pthread_t tid;
        if (0 > pthread_create(&tid, NULL, &__sta_vpn_worker, args)){
            perror("pthread_create()");
            free(args);
            sta_sock_close(client);
            continue;
        }

        pthread_detach(tid);
    }
    
    return 0;
}

void sta_vpn_serv_stop(sta_vpnserver *serv){
    sta_nserv_stop(&serv->lserver);
}

#endif
#define STA_VPNSERVER
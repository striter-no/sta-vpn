#include "networking/socket.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/poll.h>
#include <sys/socket.h>
#ifndef STA_SOCKS5_SERV
#include <networking/server.h>
#include <networking/client.h>
#include "types.h"

#include <sys/types.h>
#include <pthread.h>
#include <stdio.h>
#include <netdb.h>
#include <netdb.h>

#include <stdlib.h>
#include <poll.h>

typedef struct {
    sta_nserv proxy_server;

    // ...
} sta_socks5_proxy;

typedef struct {
    char           ip[24];
    unsigned short port;
} ip_address;

typedef struct {
    char           domain[UINT8_MAX + 1];
    unsigned short port;
    int            domain_len;
} domain_address;

typedef union {
    ip_address     ip;
    domain_address domain;
} sta_addru;

typedef struct {
    sta_addru address;
    int       addrtype;
} sta_addr;

int __stas5_recv_string(sta_stream stream, char *str) {
    uint8_t len;
    if (stream_read_exact(stream, sizeof(uint8_t), &len) != 0) {
        return -1;
    }
    if (stream_read_exact(stream, len, str) != 0) {
        return -1;
    }
    str[len] = '\0';
    return len;
}

void __stas5_send_server_hello(sta_stream stream, uint8_t method) {
    socks5_server_hello_t server_hello = {
            .version = SOCKS5_VERSION,
            .method = method,
    };
    stream_write_exact(stream, sizeof(socks5_server_hello_t), &server_hello);
}

int __stas5_handle_greeting(sta_stream stream) {
    socks5_client_hello_t client_hello;
    if (stream_read_exact(stream, sizeof(socks5_client_hello_t), &client_hello) != 0) {
        return -1;
    }

    if (client_hello.version != SOCKS5_VERSION) {
        fprintf(stderr, "Unsupported socks version %#02x\n", client_hello.version);
        return -1;
    }
    uint8_t methods[UINT8_MAX];
    if (stream_read_exact(stream, client_hello.num_methods, methods) != 0) {
        return -1;
    }
    // Find server auth method in client's list
    int found = 0;
    for (int i = 0; i < (int) client_hello.num_methods; i++) {
        if (methods[i] == SOCKS5_AUTH_NO_AUTH) {
            // Find auth method in client's supported method list
            found = 1;
            break;
        }
    }
    if (!found) {
        // No acceptable method
        fprintf(stderr, "No acceptable method from client\n");
        __stas5_send_server_hello(stream, SOCKS5_AUTH_NOT_ACCEPT);
        return -1;
    }
    // Send auth method choice
    __stas5_send_server_hello(stream, SOCKS5_AUTH_NO_AUTH);
    return 0;
}

void __stas5_send_domain_reply(sta_stream stream, uint8_t reply_type, const char *domain, uint8_t domain_len, in_port_t port) {
    uint8_t buffer[sizeof(uint8_t) + UINT8_MAX + sizeof(in_port_t)];
    uint8_t *ptr = buffer;
    *(socks5_reply_t *) ptr = (socks5_reply_t) {
            .version = SOCKS5_VERSION,
            .reply = reply_type,
            .reserved = 0,
            .addr_type = SOCKS5_ATYP_DOMAIN_NAME
    };
    ptr += sizeof(socks5_reply_t);
    *ptr = domain_len;
    ptr += sizeof(uint8_t);
    memcpy(ptr, domain, domain_len);
    ptr += domain_len;
    *(in_port_t *) ptr = port;
    ptr += sizeof(in_port_t);
    stream_write_exact(stream, ptr - buffer, buffer);
}

void __stas5_send_ip_reply(sta_stream stream, uint8_t reply_type, in_addr_t ip, in_port_t port) {
    uint8_t buffer[sizeof(socks5_reply_t) + sizeof(in_addr_t) + sizeof(in_port_t)];
    uint8_t *ptr = buffer;
    *(socks5_reply_t *) ptr = (socks5_reply_t) {
            .version = SOCKS5_VERSION,
            .reply = reply_type,
            .reserved = 0,
            .addr_type = SOCKS5_ATYP_IPV4
    };
    ptr += sizeof(socks5_reply_t);
    *(in_addr_t *) ptr = ip;
    ptr += sizeof(in_addr_t);
    *(in_port_t *) ptr = port;
    stream_write_exact(stream, sizeof(buffer), buffer);
}

int sta_socks5_handle_request(sta_stream stream, sta_addr *out_addr) {
    // Handle socks request
    socks5_request_t req;
    
    if (stream_read_exact(stream, sizeof(socks5_request_t), &req) != 0) {
        return -1;
    }

    printf("just read request\n");
    if (req.version != SOCKS5_VERSION) {
        fprintf(stderr, "Unsupported socks version %#02x\n", req.version);
        return -1;
    }
    if (req.command != SOCKS5_CMD_CONNECT) {
        fprintf(stderr, "Unsupported command %#02x\n", req.command);
        return -1;
    }

    
    if (req.addr_type == SOCKS5_ATYP_IPV4) {
        in_addr_t ip;
        if (stream_read_exact(stream, sizeof(in_addr_t), &ip) != 0) {
            return -1;
        }
        in_port_t port;
        if (stream_read_exact(stream, sizeof(in_port_t), &port) != 0) {
            return -1;
        }

        strcpy(out_addr->address.ip.ip, inet_ntoa((struct in_addr){.s_addr = ip}));
        out_addr->address.ip.port = ntohs(port);
        out_addr->addrtype = 0;
        return 0;
    } else if (req.addr_type == SOCKS5_ATYP_DOMAIN_NAME) {
        // Get domain name
        char domain[UINT8_MAX + 1];
        int domain_len = __stas5_recv_string(stream, domain);
        if (domain_len <= 0) {
            return -1;
        }
        // Get port
        in_port_t port;
        if (stream_read_exact(stream, sizeof(in_port_t), &port) != 0) {
            return -1;
        }

        strcpy(out_addr->address.domain.domain, domain);
        out_addr->address.domain.port = ntohs(port);
        out_addr->address.domain.domain_len = domain_len;
        out_addr->addrtype = 1;
        return 0;
    }

    fprintf(stderr, "Unsupported address type %#02x\n", req.addr_type);
    return -1;
}

int sta_socks5_create(
    const char *bind_ip, 
    unsigned short port, 
    size_t max_clients,

    sta_socks5_proxy *proxy
){
    sta_nserv serv;
    if (0 > sta_nserv_create(bind_ip, port, max_clients, &serv)){
        return -1;
    }

    *proxy = (sta_socks5_proxy){serv};
    return 0;
}

int sta_socks5_bindnlisten(
    sta_socks5_proxy proxy
){
    return sta_nserv_bindnlisten(proxy.proxy_server);
}

int sta_socks5_pclose(sta_socks5_proxy *proxy){
    return sta_nserv_stop(&proxy->proxy_server);
}

// * resolves domain-typed addresses
// * performs connection to the address, returns connected socket
int sta_resolve_address(sta_addr address, sta_sock *sock){
    if (!sock) return -1;

    switch (address.addrtype){
        case 0: {
            sock->fd = socket(AF_INET, SOCK_STREAM, 0);
            if (sock->fd < 0) {
                perror("resolve:socket()");
                return -1;
            }

            sock->addr.sin_family = AF_INET;
            sock->addr.sin_addr.s_addr = inet_addr(address.address.ip.ip);
            sock->addr.sin_port = htons(address.address.ip.port);

            sock->len = sizeof(sock->addr);
            if (connect(sock->fd, (struct sockaddr *) &sock->addr, sock->len) < 0) {
                perror("resolve:connect()");
                close(sock->fd);
                return -1;
            }
            strcpy(sock->ip, inet_ntoa(sock->addr.sin_addr));
            sock->port = ntohs(sock->addr.sin_port); 

        } break; // IP Address
        case 1: {
            domain_address addr = address.address.domain;

            struct addrinfo hints = {0};
            hints.ai_family = AF_INET; // IPv4
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            char port_s[8];
            sprintf(port_s, "%d", addr.port);
            struct addrinfo *addr_info;
            if (getaddrinfo(addr.domain, port_s, NULL, &addr_info) != 0) {
                perror("getaddrinfo()");
                return -1;
            }
            
            // Try connecting to host
            int remote_fd = -1;
            for (struct addrinfo *ai = addr_info; ai != NULL; ai = ai->ai_next) {
                int try_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
                if (try_fd == -1) continue;
                if (connect(try_fd, ai->ai_addr, ai->ai_addrlen) == 0) {
                    remote_fd = try_fd;
                    sock->fd = remote_fd;

                    memcpy(&sock->addr, ai->ai_addr, ai->ai_addrlen);
                    sock->len = sizeof(sock->addr);

                    strcpy(sock->ip, inet_ntoa(sock->addr.sin_addr));
                    sock->port = ntohs(sock->addr.sin_port);
                    break;
                }
                close(try_fd);
            }
            freeaddrinfo(addr_info);

            if (remote_fd == -1) {
                fprintf(stderr, "Cannot connect to remote address %s:%d\n", addr.domain, addr.port);
                return -1;
            }
        } break; // Domain address
        
        default: return -1;
    }

    return 0;
}

struct __sta_socks5_wrapper_args {
    int (*wrapped)(sta_sock client, sta_sock remote, int who, void *state_holder);
    void (*connection_handler)(sta_sock client, sta_addr rem_address, sta_sock *output_socket, void *state_holder);
    sta_sock client;
    atomic_bool *is_active;
    void *state_holder;
};

void *__sta_socks5_tunnel_wrapper(void *_args){
    struct __sta_socks5_wrapper_args *args = _args;
    sta_stream cli_stream = sta_stream_create(&args->client);

    if (__stas5_handle_greeting(cli_stream) != 0){
        sta_sock_close(args->client);
        free(args);
        return NULL;
    }

    printf("tunnel for %s:%d\n", args->client.ip, args->client.port);
    sta_addr raddr;
    if (0 > sta_socks5_handle_request(cli_stream, &raddr)){
        printf("request not handled\n");
        goto err;
    }
    
    sta_sock remote;
    args->connection_handler(args->client, raddr, &remote, args->state_holder);
    if (raddr.addrtype == 0){
        __stas5_send_ip_reply(
            cli_stream, 
            remote.fd >= 0 ? SOCKS5_REP_SUCCESS: SOCKS5_REP_GENERAL_FAILURE, 
            inet_addr(raddr.address.ip.ip),
            htons(raddr.address.ip.port)
        );
    } else {
        __stas5_send_domain_reply(
            cli_stream, 
            remote.fd >= 0 ? SOCKS5_REP_SUCCESS: SOCKS5_REP_GENERAL_FAILURE, 
            raddr.address.domain.domain, 
            raddr.address.domain.domain_len, 
            htons(raddr.address.domain.port)
        );
    }

    if (remote.fd <= 0) {
        printf("connection not handled\n");
        goto err;
    }
    struct pollfd fds[2] = {
        {.fd = args->client.fd, .events = POLLIN},
        {.fd = remote.fd,       .events = POLLIN}
    };

    while (atomic_load(args->is_active)){
        printf("polling...\n");
        int ret = poll(fds, 2, 100);
        if (ret == -1) {
            perror("tunnel_wrapper:poll()");
            break;
        }

        
        if (ret == 0) continue;
        printf("polling, has new events...\n");
        for (size_t i = 0; i < 2; i++){
            if (fds[i].revents & POLLIN){
                if (0 > args->wrapped(args->client, remote, i, args->state_holder)){
                    printf("wrapped failed\n");
                    goto tunbreak;
                }
            } else if (fds[i].revents & (POLLERR | POLLHUP)){
                printf("POLLERR | POLLHUP\n");
                goto tunbreak;
            }
        }

        continue;
        tunbreak: break;
    }

err:
    sta_sock_close(remote);
    sta_sock_close(args->client);
    free(args);
    return NULL;
}

// * blocking `while(working)`
// * spawns detached threads with `tunnel_loop`
// * place it into the task if you have any logic 
//   after accept loop and outside the connection logic
int sta_socks5_accept_loop(
    sta_socks5_proxy *proxy,
    int (*tunnel_iter)(sta_sock client, sta_sock remote, int who, void *state_holder),
    void (*connection_handler)(sta_sock client, sta_addr rem_address, sta_sock *output_socket, void *state_holder),
    void *state_holder
){
    sta_nserv *server = &proxy->proxy_server;

    while (atomic_load(&proxy->proxy_server.is_active)){
        printf("accepting...\n");
        sta_sock client;
        if (0 > sta_nserv_accept(*server, &client)){
            perror("sta_socks5_accept_loop:sta_nserv_accept()");
            continue;
        }

        printf("accepted\n");
        if (0 > sta_sock_qsetopt(client, TCP_NODELAY, 1)){
            perror("sta_socks5_accept_loop:sta_sock_qsetopt()");
            continue;
        }

        pthread_t client_tid;
        struct __sta_socks5_wrapper_args *args = malloc(sizeof(struct __sta_socks5_wrapper_args));
        if (args == NULL){
            perror("sta_socks5_accept_loop:malloc()");
            continue;
        }

        *args = (struct __sta_socks5_wrapper_args){
            .client = client,
            .wrapped = tunnel_iter,
            .connection_handler = connection_handler,
            .is_active = &server->is_active,
            .state_holder = state_holder
        };

        printf("creating thread\n");
        if (pthread_create(
            &client_tid, 
            NULL, 
            &__sta_socks5_tunnel_wrapper, 
            args
        ) != 0) {
            perror("pthread_create()");
            sta_sock_close(client);
            free(args);
            continue;
        }

        pthread_detach(client_tid);
    }
}

#endif
#define STA_SOCKS5_SERV
#include <networking/server.h>
#include <stdint.h>
#include <stdio.h>

int main(){
    const char           ip[] = "127.0.0.1";
    const unsigned short port = 9000;

    sta_nserv server;
    if (0 > sta_nserv_create(ip, port, 2, &server)){
        perror("sta_nserv_create()");
        return -1;
    }

    if (0 > sta_nserv_bindnlisten(server)){
        perror("sta_nserv_bindnlisten");
        return -1;
    }

    sta_sock client;
    if (0 > sta_nserv_accept(server, &client)){
        perror("sta_nserv_accept");
        return -1;
    }

    sta_stream stream = sta_stream_create(&client);
    uint8_t len;
    if (0 > stream_read_exact(stream, sizeof(uint8_t), &len)){
        perror("stream_read_exact");
        return -1;
    }

    char buffer[1024] = {0};
    if (0 > stream_read_exact(stream, len, buffer)){
        perror("stream_read_exact");
        return -1;
    }

    printf("just got: %s\n", buffer);

    // echoing

    if (0 > stream_write_exact(stream, sizeof(uint8_t), &len)){
        perror("stream_write_exact");
        return -1;
    }

    if (0 > stream_write_exact(stream, len, buffer)){
        perror("stream_write_exact");
        return -1;
    }

    sta_sock_close(client);
    sta_nserv_stop(&server);
}
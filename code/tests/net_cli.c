#include <networking/client.h>
#include <stdio.h>
#include <stdint.h>

int main(void){
    const char     ip[] = "127.0.0.1";
    unsigned short port = 9000;

    sta_ncli client;
    if (0 > sta_ncli_create(ip, port, &client)){
        perror("sta_ncli_create()");
        return -1;
    }

    if (0 > sta_ncli_connect(client)){
        perror("sta_ncli_connect()");
        return -1;
    }

    const char message[] = "hello world";
    uint8_t len = strlen(message) + 1;

    sta_stream stream = sta_ncli_stream(&client);
    if (0 > stream_write_exact(stream, sizeof(uint8_t), &len)){
        perror("stream_write_exact()");
        return -1;
    }
    if (0 > stream_write_exact(stream, len, (void*)message)){
        perror("stream_write_exact()");
        return -1;
    }

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

    sta_ncli_disconnect(client);
}
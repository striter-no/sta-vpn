#include "socket.h"
#include "errno.h"
#include "unistd.h"

#ifndef STA_NSTREAM

typedef struct {
    void *origin;
    int fd;
} sta_stream;

sta_stream sta_stream_create(sta_sock *sock){
    return (sta_stream){
        .fd = sock->fd,
        .origin = sock
    };
}

// * blocking read() function
// * buffer needs to be already allocated
int stream_read_exact(sta_stream str, size_t bytes, void *buffer){
    size_t remain = bytes;
    while (remain > 0) {
        ssize_t write_len = read(str.fd, buffer, remain);
        if (write_len < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                // EINTR: interrupted by system
                // EAGAIN: read is blocked
                // Try again
                continue;
            }
            return -1; // Unexpected error
        } else if (write_len == 0) {
            return -1; // No data from socket: disconnected.
        } else {
            // read success
            remain -= write_len;
            buffer += write_len;
        }
    }

    return 0;
}

// * blocking write() function
int stream_write_exact(sta_stream str, size_t bytes, void *buffer){
    size_t remain = bytes;
    while (remain > 0) {
        ssize_t write_len = write(str.fd, buffer, remain);
        if (write_len < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                // EINTR: interrupted by system
                // EAGAIN: write is blocked
                // Try again
                continue;
            }
            return -1; // Unexpected error
        } else if (write_len == 0) {
            return -1; // No data from socket: disconnected.
        } else {
            // write success
            remain -= write_len;
            buffer += write_len;
        }
    }
    
    return 0;
}

#endif
#define STA_NSTREAM
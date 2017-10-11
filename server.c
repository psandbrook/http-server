#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BUF_SIZE 1024

static void* client_thread_start(void* sock_ptr) {
    int sock = *(int*)sock_ptr;
    free(sock_ptr);

    while (true) {
        char buf[BUF_SIZE];
        ssize_t n_read = read(sock, &buf, sizeof(buf) - 1);
        if (n_read == -1) {
            perror("read() failed");
            break;
        } else if (n_read == 0) {
            break;
        }

        buf[n_read] = '\0';
        fputs(buf, stdout);
    }

    close(sock);
    return NULL;
}

int main(int argc, char** argv) {
    int ret = 0;

    if (argc < 2) {
        fprintf(stderr, "No port given\n");
        ret = 1;
        goto error;
    }

    // Get the port number
    char* strtol_r = NULL;
    long port_l = strtol(argv[1], &strtol_r, 0);
    if (port_l < 0 || port_l > UINT16_MAX || strtol_r == argv[1]) {
        fprintf(stderr, "Port is not valid\n");
        ret = 1;
        goto error;
    }

    uint16_t port = port_l;

    int lis_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (lis_sock == -1) {
        perror("socket() failed");
        ret = 1;
        goto error;
    }

    // Bind to any address on the given port
    struct sockaddr_in6 addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(port),
        .sin6_flowinfo = 0,
        .sin6_addr = in6addr_any,
        .sin6_scope_id = 0,
    };

    int bind_r = bind(lis_sock, (const struct sockaddr*)&addr, sizeof(addr));
    if (bind_r != 0) {
        perror("bind() failed");
        ret = 1;
        goto error_lis_sock;
    }

    int listen_r = listen(lis_sock, 128);
    if (listen_r != 0) {
        perror("listen() failed");
        ret = 1;
        goto error_lis_sock;
    }

    bool end = false;
    while (!end) {

        int c_sock = accept(lis_sock, NULL, NULL);
        if (c_sock == -1) {
            perror("accept() failed");
            switch (errno) {
            case ECONNABORTED:
            case EINTR:
            case EPROTO:
                // Retry
                continue;
            default:
                end = true;
                goto error_lis_sock;
            }
        }

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        int* c_sock_ptr = malloc(sizeof(int));
        if (c_sock_ptr == NULL) {
            perror("malloc() failed");
            end = true;
            goto error_attr;
        }

        *c_sock_ptr = c_sock;
        c_sock = -1;

        pthread_t c_thread;
        int create_r = pthread_create(&c_thread, &attr, client_thread_start, c_sock_ptr);
        if (create_r != 0) {
            errno = create_r;
            perror("pthread_create() failed");
            end = true;
            goto error_attr;
        }

error_attr:
        pthread_attr_destroy(&attr);
        close(c_sock);
    }

error_lis_sock:
    close(lis_sock);
error:
    return ret;
}

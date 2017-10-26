#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "client_thread.h"

// Converts a string to an unsigned 16-bit integer.
// On success, `*out` is set to the result and `true` is returned. On failure,
// `false` is returned.
static bool str_to_u16(const char* str, uint16_t* out) {
    char* strtol_r = NULL;
    long out_l = strtol(str, &strtol_r, 0);
    if (out_l < 0 || out_l > UINT16_MAX || strtol_r == str) {
        return false;
    }

    *out = out_l;
    return true;
}

int main(int argc, char** argv) {

    // Get port number /////////////////////////////////////////////////////////

    if (argc < 2) {
        fprintf(stderr, "No port given\n");
        return 1;
    }

    uint16_t port;
    if (!str_to_u16(argv[1], &port)) {
        fprintf(stderr, "Port is not valid\n");
        return 1;
    }

    // Get a listening socket //////////////////////////////////////////////////

    int lis_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (lis_sock == -1) {
        perror("socket");
        return 1;
    }

    int reuse_addr = 1;
    // Ignore failure
    setsockopt(lis_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

    // Bind to any address on the given port
    struct sockaddr_in6 addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(port),
        .sin6_flowinfo = 0,
        .sin6_addr = in6addr_any,
        .sin6_scope_id = 0,
    };

    if (bind(lis_sock, (const struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("bind");
        return 1;
    }

    if (listen(lis_sock, 128) != 0) {
        perror("listen");
        return 1;
    }

    // Listen for incoming connections /////////////////////////////////////////

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    while (true) {
        int c_sock = accept(lis_sock, NULL, NULL);
        if (c_sock == -1) {
            perror("accept");
            switch (errno) {
            case ECONNABORTED:
            case EINTR:
            case EPROTO:
                // Retry
                fprintf(stderr, "Retrying...\n");
                continue;
            default:
                return 1;
            }
        }

        int* c_sock_ptr = malloc(sizeof(int));
        if (c_sock_ptr == NULL) {
            perror("malloc");
            return 1;
        }

        *c_sock_ptr = c_sock;

        pthread_t c_thread;
        int create_r = pthread_create(&c_thread, &attr, client_thread_start, c_sock_ptr);
        if (create_r != 0) {
            errno = create_r;
            perror("pthread_create");
            return 1;
        }
    }

    return 0;
}

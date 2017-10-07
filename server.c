#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUF_SIZE 1024

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

    int con_sock = accept(lis_sock, NULL, NULL);
    if (con_sock == -1) {
        perror("accept() failed");
        ret = 1;
        goto error_lis_sock;
    }

    close(lis_sock);
    lis_sock = -1;

    while (true) {
        char buf[BUF_SIZE];
        ssize_t n_read = read(con_sock, &buf, sizeof(buf) - 1);
        if (n_read == 0) {
            break;
        } else if (n_read == -1) {
            perror("read() failed");
            ret = 1;
            goto error_con_sock;
        }

        buf[n_read] = '\0';
        fputs(buf, stdout);
    }

error_con_sock:
    close(con_sock);
error_lis_sock:
    close(lis_sock);
error:
    return ret;
}

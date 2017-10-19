#include "client_thread.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "mystring.h"

#define CRLF "\r\n"
#define CHUNK_SIZE 1024

static void print_oom(void) {
    fprintf(stderr, "Out of memory\n");
}

static void print_http_req_err(void) {
    fprintf(stderr, "Invalid HTTP request\n");
}

// Returns true if the null-terminated string `start` is a prefix of the
// null-terminated string `str`, false otherwise.
static bool str_start(const char* str, const char* start) {
    if (strncmp(str, start, strlen(start)) == 0) {
        return true;
    } else {
        return false;
    }
}

// Like `fclose`, but is a no-op if `file` is `NULL`.
static void fclose_c(FILE* file) {
    if (file != NULL) {
        fclose(file);
    }
}

void* client_thread_start(void* sock_ptr) {
    int sock = *(int*)sock_ptr;
    free(sock_ptr);

    String request = new_string();
    String response = new_string();
    FILE* file = NULL;

    // Read HTTP request ///////////////////////////////////////////////////////

    while (true) {
        if (!reserve(&request, CHUNK_SIZE + 1)) {
            print_oom();
            goto cleanup;
        }

        if (request.len == 0) {
            request.ptr[0] = '\0';
            request.len = 1;
        }

        ssize_t n_read = read(sock, &request.ptr[request.len - 1], CHUNK_SIZE);
        if (n_read == -1) {
            perror("read");
            goto cleanup;
        } else if (n_read == 0) {
            print_http_req_err();
            goto cleanup;
        }

        request.len += n_read;
        request.ptr[request.len - 1] = '\0';
        if (strstr(request.ptr, CRLF CRLF)) { break; }
    }

    // Parse HTTP request //////////////////////////////////////////////////////

    // Check method
    const char* method_str = "GET ";
    if (!str_start(request.ptr, method_str)) {
        print_http_req_err();
        goto cleanup;
    }

    char* path = &request.ptr[strlen(method_str)];
    char* sp_ptr = strchr(path, ' ');
    if (sp_ptr == NULL) {
        print_http_req_err();
        goto cleanup;
    }

    *sp_ptr = '\0';

    // Check version
    char* http_ver = sp_ptr + 1;
    if (!str_start(http_ver, "HTTP/1.1" CRLF)) {
        print_http_req_err();
        goto cleanup;
    }

    size_t path_len = strlen(path);
    if (path[path_len - 1] == '/') {
        // If the path ends with a `/`, it's a directory
        strcpy(&path[path_len], "index.html");
    }

    // Remove leading slash
    path = &path[1];

    printf("Received HTTP GET request for %s\n", path);

    // Construct HTTP response /////////////////////////////////////////////////

    file = fopen(path, "rb");
    if (file == NULL) {
        switch (errno) {

        case EACCES: {
            bool append_r = append(&response,
                "HTTP/1.1 403 Forbidden" CRLF
                "Connection: close" CRLF
                CRLF
            );
            if (!append_r) {
                print_oom();
                goto cleanup;
            }

            file = fopen("403-forbidden.html", "rb");
            if (file == NULL) {
                perror("fopen");
                goto cleanup;
            }

            break;
         }

        case ENOENT:
        case ENOTDIR: {
            bool append_r = append(&response,
                "HTTP/1.1 404 Not Found" CRLF
                "Connection: close" CRLF
                CRLF
            );
            if (!append_r) {
                print_oom();
                goto cleanup;
            }

            file = fopen("404-not-found.html", "rb");
            if (file == NULL) {
                perror("fopen");
                goto cleanup;
            }

            break;
        }

        default:
            perror("fopen");
            goto cleanup;
        }
    } else {
        bool append_r = append(&response,
            "HTTP/1.1 200 OK" CRLF
            "Connection: close" CRLF
            CRLF
        );
        if (!append_r) {
            print_oom();
            goto cleanup;
        }
    }

    // Read file into response body
    while (true) {
        if (!reserve(&response, CHUNK_SIZE)) {
            print_oom();
            goto cleanup;
        }

        size_t n_read = fread(&response.ptr[response.len], 1, CHUNK_SIZE, file);
        response.len += n_read;
        if (feof(file) != 0) {
            break;
        } else if (ferror(file) != 0) {
            perror("fread");
            goto cleanup;
        }
    }

    fclose_c(file);
    file = NULL;

    // Write HTTP response /////////////////////////////////////////////////////

    char* remain_ptr = response.ptr;
    size_t remain_len = response.len;
    while (true) {

        ssize_t n_written = write(sock, remain_ptr, remain_len);
        if (n_written == -1) {
            perror("write");
            goto cleanup;
        } else if (n_written == 0) {
            fprintf(stderr, "Could not write HTTP response\n");
            goto cleanup;
        }

        remain_len -= n_written;
        if (remain_len == 0) { break; }
        remain_ptr = &remain_ptr[n_written];
    }

    // Cleanup /////////////////////////////////////////////////////////////////

cleanup:
    fclose_c(file);
    destroy_string(&response);
    destroy_string(&request);
    close(sock);
    return NULL;
}

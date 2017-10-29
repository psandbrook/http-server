#include "client_thread.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mystring.h"

#define CRLF "\r\n"
#define CHUNK_SIZE 1024

static void print_http_req_err(void) {
    fprintf(stderr, "Invalid HTTP request\n");
}

// Returns true if the null-terminated string `start` is a prefix of the
// null-terminated string `str`, false otherwise.
static bool str_start(const char* str, const char* start) {
    return strncmp(str, start, strlen(start)) == 0;
}

// Converts a string to an unsigned long.  On success, `*out` is set to the
// result and `true` is returned. On failure, `false` is returned.
static bool str_to_ul(const char* str, unsigned long* out) {
    char* strtol_r = NULL;
    long out_l = strtol(str, &strtol_r, 0);
    if (out_l < 0 || strtol_r == str) { return false; }
    *out = out_l;
    return true;
}

// Like `fclose`, but is a no-op if `file` is `NULL`.
static void fclose_c(FILE* file) {
    if (file != NULL) {
        fclose(file);
    }
}

// Like `close`, but is a no-op if `fd` is negative.
static void close_c(int fd) {
    if (fd >= 0) {
        close(fd);
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
        reserve(&request, CHUNK_SIZE + 1);

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
    bool include_file;
    char* path;
    if (str_start(request.ptr, "GET ")) {
        include_file = true;
        path = &request.ptr[4];
    } else if (str_start(request.ptr, "HEAD ")) {
        include_file = false;
        path = &request.ptr[5];
    } else {
        print_http_req_err();
        goto cleanup;
    }

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

    if (path[strlen(path) - 1] == '/') {
        // If the path ends with a `/`, it's a directory
        strcat(path, "index.html");
    }

    // Remove leading slash
    path = &path[1];

    // Check for `Range` field
    bool range = false;
    unsigned long range_from = 0;
    unsigned long range_to = 0;
    char* range_str = strstr(&path[strlen(path) + 1], "Range: bytes=");
    if (range_str != NULL) {
        char* from_str = &range_str[13];
        char* to_str = strchr(range_str, '-');
        if (to_str != NULL) {
            to_str = &to_str[1];
            if (str_to_ul(from_str, &range_from) && str_to_ul(to_str, &range_to)) {
                range = true;
            }
        }
    }

    if (include_file) {
        printf("Received HTTP GET request for %s\n", path);
    } else {
        printf("Received HTTP HEAD request for %s\n", path);
    }

    // Construct HTTP response /////////////////////////////////////////////////

    file = fopen(path, "rb");
    if (file == NULL) {

        const char* new_path;
        switch (errno) {

        case EACCES:
            append(&response, "HTTP/1.1 403 Forbidden" CRLF);
            new_path = "403-forbidden.html";
            break;

        case ENOENT:
        case ENOTDIR:
            append(&response, "HTTP/1.1 404 Not Found" CRLF);
            new_path = "404-not-found.html";
            break;

        default:
            perror("fopen");
            goto cleanup;
        }

        file = fopen(new_path, "rb");
        if (file == NULL) {
            perror("fopen");
            goto cleanup;
        }

    } else if (range) {
        // Partial Content response

        struct stat file_stat;
        if (stat(path, &file_stat) == -1) {
            perror("stat");
            goto cleanup;
        }

        unsigned long file_size = file_stat.st_size;
        if (range_to < range_from || range_to >= file_size) {
            // Range is not valid
            append(&response, "HTTP/1.1 416 Range Not Satisfiable" CRLF);
            include_file = false;

        } else {
            append(&response,
                "HTTP/1.1 206 Partial Content" CRLF
                "Content-Range: bytes ");

            int val_size = snprintf(NULL, 0, "%lu-%lu/%lu", range_from, range_to, file_size);
            reserve(&response, val_size + 1);
            sprintf(&response.ptr[response.len], "%lu-%lu/%lu", range_from ,range_to, file_size);
            response.len += val_size;
            append(&response, CRLF);
        }

    } else {
        append(&response, "HTTP/1.1 200 OK" CRLF);
    }

    append(&response,
        "Connection: close" CRLF
        "Accept-Ranges: bytes" CRLF
        CRLF);

    // Read file into response body
    if (include_file) {
        if (range) {
            size_t remain_len = (range_to + 1) - range_from;
            if (fseek(file, range_from, SEEK_SET) != 0) {
                perror("fseek");
                goto cleanup;
            }

            reserve(&response, remain_len);
            while (true) {
                size_t n_read = fread(&response.ptr[response.len], 1, remain_len, file);
                if (feof(file) != 0) {
                    fprintf(stderr, "Unexpected EOF\n");
                    goto cleanup;
                } else if (ferror(file) != 0) {
                    perror("fread");
                    goto cleanup;
                }

                response.len += n_read;
                remain_len -= n_read;
                if (remain_len == 0) { break; }
            }

        } else {
            while (true) {
                reserve(&response, CHUNK_SIZE);
                size_t n_read = fread(&response.ptr[response.len], 1, CHUNK_SIZE, file);
                response.len += n_read;
                if (feof(file) != 0) {
                    break;
                } else if (ferror(file) != 0) {
                    perror("fread");
                    goto cleanup;
                }
            }
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
    close_c(sock);
    return NULL;
}

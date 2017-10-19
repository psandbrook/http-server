#include "misc.h"

#include <unistd.h>

void close_c(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

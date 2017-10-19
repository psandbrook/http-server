#ifndef MISC_H
#define MISC_H

// Like `close`, but is a no-op if `fd` is negative.
void close_c(int fd);

#endif

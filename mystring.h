#ifndef MYSTRING_H
#define MYSTRING_H

#include <stdbool.h>
#include <stdlib.h>

// A heap-allocated array of characters.
typedef struct {
    char* ptr;
    size_t cap;
    size_t len;
} String;

String new_string(void);
void destroy_string(String* self);

// Reserve extra capacity for `add` more characters.
bool reserve(String* self, size_t add);

// Append the null-terminated string `str` to `self`. The terminating null
// character is not included.
bool append(String* self, const char* str);

#endif

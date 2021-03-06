#include "mystring.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t max(size_t x, size_t y) {
    if (x > y) {
        return x;
    } else {
        return y;
    }
}

String new_string(void) {
    return (String){ .ptr = NULL, .cap = 0, .len = 0 };
}

void destroy_string(String* self) {
    assert(self != NULL);
    free(self->ptr);
}

void reserve(String* self, size_t add) {
    assert(self != NULL);
    size_t new_len = self->len + add;

    if (self->cap < new_len) {
        size_t new_cap = max(new_len, self->cap * 1.5);
        char* new_ptr = realloc(self->ptr, new_cap);
        if (new_ptr == NULL) {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }

        self->cap = new_cap;
        self->ptr = new_ptr;
    }
}

void append(String* self, const char* str) {
    assert(self != NULL);
    size_t str_len = strlen(str);
    reserve(self, str_len);
    memcpy(&self->ptr[self->len], str, str_len);
    self->len += str_len;
}

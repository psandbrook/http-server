CC := gcc
WARN_CFLAGS := \
	-Wall -Wextra -pedantic -Wcast-align -Wcast-qual -Wunused \
	-Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op \
	-Wmissing-declarations -Wmissing-include-dirs -Wredundant-decls -Wshadow \
	-Wstrict-overflow=5 -Wundef -Wfloat-equal

CFLAGS := $(WARN_CFLAGS) -std=c99 -pthread -O3 -g0

SOURCES := server.c client_thread.c mystring.c
OBJECTS := $(SOURCES:.c=.o)
BIN := server

.PHONY: all clean
all: $(BIN)

clean:
	rm -f $(OBJECTS) $(BIN)

$(BIN): $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $^

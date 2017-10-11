CC := gcc
WARN_CFLAGS := \
	-Wall -Wextra -pedantic -Wcast-align -Wcast-qual -Wunused \
	-Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op \
	-Wmissing-declarations -Wmissing-include-dirs -Wredundant-decls -Wshadow \
	-Wstrict-overflow=5 -Wundef -Wfloat-equal -Wabi

DEBUG_CFLAGS := -O0 -g
RELEASE_CFLAGS := -O3 -g0 -DNDEBUG

#CFLAGS := $(WARN_CFLAGS) -std=c99 -MMD -MP
CFLAGS := $(WARN_CFLAGS) -std=c99 -pthread
ifdef DEBUG
	CFLAGS += $(DEBUG_CFLAGS)
else
	CFLAGS += $(RELEASE_CFLAGS)
endif

SOURCES := server.c
OBJECTS := $(SOURCES:.c=.o)
#DEPS := $(SOURCES:.c=.d)
BIN := server

.PHONY: all clean
all: $(BIN)

clean:
#rm -f $(OBJECTS) $(DEPS) $(BIN)
	rm -f $(OBJECTS) $(BIN)

$(BIN): $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $^

#-include $(DEPS)

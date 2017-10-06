CC := gcc
WARN_CFLAGS := \
	-Wpedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wunused \
	-Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op \
	-Wmissing-declarations -Wmissing-include-dirs -Wredundant-decls -Wshadow \
	-Wstrict-overflow=5 -Wundef -Wfloat-equal -Wabi -Wstack-protector

DEBUG_CFLAGS := -O0 -ggdb3 -fsanitize=undefined
RELEASE_CFLAGS := \
	-O3 -DNDEBUG -flto -fuse-linker-plugin -fstack-protector-strong --param \
	ssp-buffer-size=4 -fPIE -pie

#CFLAGS := $(WARN_CFLAGS) -std=c99 -MMD -MP
CFLAGS := $(WARN_CFLAGS) -std=c99
ifdef DEBUG
	CFLAGS += $(DEBUG_CFLAGS)
	STRIP := @:
else
	CFLAGS += $(RELEASE_CFLAGS)
	STRIP := strip
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
	$(STRIP) --strip-unneeded $@

#-include $(DEPS)

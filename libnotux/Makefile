# Makefile for building libnotux static library

CC := gcc
AR := ar
CFLAGS := -m64 -ffreestanding -fno-stack-protector -mno-red-zone -mcmodel=large \
          -fno-pic -fno-pie -Iinclude -std=c11 -Wall -Wextra \
          -Wno-unused-parameter -Wno-unused-function \
          -Wno-missing-field-initializers -O2 -g

# Source files for libnotux
LIB_SOURCES := \
    src/libc.c \
    src/printf.c \
    src/string.c \
    src/mem.c \
    src/math.c \
    src/fs.c \
    src/proc.c \
    src/net.c \
    src/term.c \
    src/time.c \
    src/dl.c

# Object files
LIB_OBJECTS := $(LIB_SOURCES:%.c=%.o)

# Library name
LIBRARY := libnotux.a

.PHONY: all clean install

all: $(LIBRARY)

$(LIBRARY): $(LIB_OBJECTS)
	@echo "  [AR]   $@"
	$(AR) rcs $@ $^

%.o: %.c
	@echo "  [CC]   $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIB_OBJECTS) $(LIBRARY)

install: $(LIBRARY)
	@mkdir -p ../build/lib
	cp $(LIBRARY) ../build/lib/

# Dependencies for object files
src/libc.o: src/libc.c include/notux/libc.h
src/printf.o: src/printf.c include/notux/libc.h
src/string.o: src/string.c include/notux/libc.h
src/mem.o: src/mem.c include/notux/libc.h
src/math.o: src/math.c include/notux/libc.h
src/fs.o: src/fs.c include/notux/libc.h
src/proc.o: src/proc.c include/notux/libc.h
src/net.o: src/net.c include/notux/libc.h
src/term.o: src/term.c include/notux/libc.h
src/time.o: src/time.c include/notux/libc.h
src/dl.o: src/dl.c include/notux/libc.h

# Create build directory structure if needed
$(shell mkdir -p build)
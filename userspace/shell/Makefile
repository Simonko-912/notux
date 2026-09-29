# Makefile for the nsh shell (links against libnotux + crt0)
CC      = gcc
CFLAGS  = -I../../libnotux/include -Wall -Wextra -O2 -m64 \
          -fno-pie -fno-pic -ffreestanding -fno-stack-protector -mno-red-zone -mcmodel=large
CRT0    = ../../build/lib/crt0.o
LDFLAGS = -T ../../libnotux/crt0.ld -L../../build/lib -lnotux \
          -nostdlib -static -no-pie

TARGET = nsh
SOURCES = nsh.c nsh_enhanced.c

$(TARGET): $(SOURCES) nsh.h $(CRT0) ../../build/lib/libnotux.a
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(CRT0) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) ../../build/bin/
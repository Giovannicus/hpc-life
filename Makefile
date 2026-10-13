CC = gcc
CFLAGS = -O2 -Wall

all: life_serial

life_serial: src/life_serial.c
	$(CC) $(CFLAGS) -o life_serial src/life_serial.c

clean:
	rm -f life_serial

run_standard: life_serial
	./life_serial 1024 100

.PHONY: all clean run_standard
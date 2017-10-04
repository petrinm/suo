CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99 -O3
LIBS=-lliquid
OBJS=burstfsk.o main.o

all: $(OBJS)
	$(CC) $(OBJS) -o main $(LIBS)

clean:
	rm main $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean

CC=gcc
CFLAGS=-g

SRCS=fs.c
OBJS=fs.o

%.o : %.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(OBJS)
	$(CC) -o doubt $^ $(CFLAGS)


clean:
	rm *.o doubt

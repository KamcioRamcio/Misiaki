CC      := mpicc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -Isrc
LDFLAGS :=
TARGET  := okrety

SRCS := \
    src/main.c \
    src/clock.c \
    src/queue.c \
    src/dock.c \
    src/mechanics.c \
    src/comm.c \
    src/log.c

OBJS := $(SRCS:.c=.o)

.PHONY: all debug clean

all: $(TARGET)

debug: CFLAGS += -DDEBUG -g -O0
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic -D_REENTRANT -pthread -lrt
DEPS= constants.h aux.h types.h sope.h user.h server.h
OBJS= log.o aux.o

.PHONY: all clean
all: server user

server: $(OBJS) server.o
	$(CC) $(CFLAGS) $^ -o $@

user: $(OBJS) user.o
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f server user *.o *.d *.i *.s

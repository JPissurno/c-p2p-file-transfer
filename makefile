CFLAGS = -Os -Wall -Wextra -Wpedantic -Wno-unused-result -Wno-unused-parameter
LDFLAGS = -pthread

CC = gcc
CMACROS = -DNW_LOG=0
SMACROS = -DNW_LOG=1
OBJC = network_utils.o client/client_main.o client/client_connection.o client/client_menus.o client/client_files.o client/client_listen.o client/client_error.o
OBJS = network_utils.o server/server_main.o server/server_connection.o server/singly.o server/server_error.o

all: cexec sexec clean

cexec: $(OBJC)
	$(CC) -o $@ $(OBJC) $(LDFLAGS)

sexec: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
	
%.o: client/%.c
	$(CC) $(CMACROS) $(CFLAGS) -c $<

%.o: server/%.c
	$(CC) $(SMACROS) $(CFLAGS) -c $<

clean:
	rm -f $(OBJC) $(OBJS)
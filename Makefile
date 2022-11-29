CC = g++
CFLAGS = -Wall -g -pthread -fpermissive
INCLUDES = -I./include

SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %cpp, %o, $(SRCS))

OTHER_OBJS = socket_exec.o common.o

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

udp_client: udp_client.o $(OTHER_OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o udp_client udp_client.o $(OTHER_OBJS)

tcp_client: tcp_client.o $(OTHER_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o tcp_client tcp_client.o $(OTHER_OBJS)

udp_server: udp_server.o $(OTHER_OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o udp_server udp_server.o $(OTHER_OBJS)

tcp_server: tcp_server.o $(OTHER_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o tcp_server tcp_server.o $(OTHER_OBJS)

clean:
	rm -rf *.o udp_client tcp_client udp_server tcp_server

all: udp_client tcp_client udp_server tcp_server
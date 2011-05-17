CC = gcc
CFLAGS = -g
LDLIBS = -lws2_32 -lstdc++
OBJECTS = module.o slotwait.o main.o proto_kad.o \
		  udp_daemon.o slotsock.o timer.o \
		  btcodec.o utils.o ui.o

all: client.exe test.exe

client.exe: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o client.exe

test.exe: test.o
	$(CC) $(LDFLAGS) test.o $(LDLIBS) -o test.exe

clean:
	rm -f $(OBJECTS)


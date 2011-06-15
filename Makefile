CC = gcc
CFLAGS = -g
CXXFLAGS = -g
LDFLAGS = -L.
LDLIBS = -lws2_32 -lstdc++ -llibmpg123-0 -lwinmm
OBJECTS = module.o slotwait.o main.o kad_proto.o kad_route.o \
		  btcodec.o utils.o ui.o recursive.o base64.o player.o \
		  udp_daemon.o slotsock.o callout.o webcrack.o slotipc.o

all: client.exe test.exe

client.exe: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o client.exe

test.exe: test.o
	$(CC) $(LDFLAGS) test.o $(LDLIBS) -o test.exe

clean:
	rm -f $(OBJECTS)


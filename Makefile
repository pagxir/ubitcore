CC = gcc
CFLAGS = -g
CXXFLAGS = -g
LDLIBS = -lws2_32 -lstdc++
OBJECTS = module.o slotwait.o main.o proto_kad.o \
		  btcodec.o utils.o ui.o recursive.o base64.o \
		  udp_daemon.o slotsock.o callout.o webcrack.o

all: client.exe test.exe

client.exe: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o client.exe

test.exe: test.o
	$(CC) $(LDFLAGS) test.o $(LDLIBS) -o test.exe

clean:
	rm -f $(OBJECTS)


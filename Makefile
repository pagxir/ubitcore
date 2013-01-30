CC = gcc
CXX = g++
CFLAGS = -g -Ilibwait/include
CXXFLAGS = -g -Ilibwait/include
LDFLAGS = -L. -Llibwait
LDLIBS = -lstdc++ -lwait -lrt
OBJECTS = main.o kad_proto.o kad_route.o \
		  btcodec.o utils.o recursive.o base64.o \
		  udp_daemon.o webcrack.o kad_store.o

all: client.exe test.exe

client.exe: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o client.exe

test.exe: test.o
	$(CC) $(LDFLAGS) test.o $(LDLIBS) -o test.exe

clean:
	rm -f $(OBJECTS)


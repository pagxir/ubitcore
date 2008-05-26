CC = gcc
CXX = g++
CXXFLAGS = -I. -g -DUTF_8
LDLIBS = 

SRC =  fsocket.cc tracker.cc peer.cc upeer.cc zpiece.cc sha1.cc fhandle.cc listener.cc main.cc netmgr.cc speedup.cc handshake.cc btdump.cc btlink.cc

OBJECTS = ${SRC:.cc=.o}

all: btload $(SRC)

btload: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o btload $(OBJECTS) $(LDLIBS)

clean:
	rm -f $(OBJECTS)


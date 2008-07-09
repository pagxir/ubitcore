CXX = g++
CXXFLAGS = -g
SRCS = bthread.cc bsocket.cc biothread.cc burlget.cc main.cc boutfile.cc bdhtnet.cc btcodec.cc sha1.cc bpeermgr.cc btracker.cc bqueue.cc bpeer.cc bclock.cc butils.cc boffer.cc bupdown.cc bchunk.cc
OBJS = ${SRCS:.cc=.o}

all:
	sh build.sh bmain

bmain: $(OBJS)
	$(CXX) $(LDFLAGS) -o bmain $(OBJS) $(LDLIBS)

.cc.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

bclean:
	rm -f $(OBJS)

clean:
	sh build.sh bclean

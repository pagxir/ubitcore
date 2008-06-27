CXX = g++
CXXFLAGS = -O2
SRCS = bthread.cc bsocket.cc biothread.cc burlget.cc main.cc boutfile.cc bdhtnet.cc btcodec.cc sha1.cc bpeermgr.cc btracker.cc bqueue.cc bpeer.cc bclock.cc butils.cc boffer.cc
OBJS = ${SRCS:.cc=.o}

bmain: $(OBJS)
	$(CXX) $(LDFLAGS) -o bmain $(OBJS) $(LDLIBS)

.cc.o:
	$(CXX) -c $< -o $@

clean:
	rm -f $(OBJS)

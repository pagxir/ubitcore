CXX = g++
CXXFLAGS = -g
SRCS = bthread.cc bsocket.cc biothread.cc burlget.cc main.cc boutfile.cc
OBJS = ${SRCS:.cc=.o}

bmain: $(OBJS)
	$(CXX) $(LDFLAGS) -o bmain $(OBJS) $(LDLIBS)

.cc.o:
	$(CXX) -c $< -o $@

clean:
	rm -f $(OBJS)

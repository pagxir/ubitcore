CXXFLAGS = -g
SRCS = main.cc bthread.cc bsocket.cc biothread.cc bhttpd.cc
OBJS = ${SRCS:.cc=.o}

bmain: $(OBJS)
	$(CXX) $(LDFLAGS) -o bmain $(OBJS) $(LDLIBS)

clean:
	rm -f $(OBJS)

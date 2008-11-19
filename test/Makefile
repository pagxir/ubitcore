CXXFLAGS = -g
all: pingdht finddht getpeersdht getpeersdht_r finddht_r get_peers

pingdht: pingdht.o btcodec.o
	$(CXX) $(LDFLAGS) -o pingdht pingdht.o btcodec.o
finddht: finddht.o btcodec.o
	$(CXX) $(LDFLAGS) -o finddht finddht.o btcodec.o
finddht_r: finddht_r.o btcodec.o
	$(CXX) $(LDFLAGS) -o finddht_r finddht_r.o btcodec.o
getpeersdht: getpeersdht.o btcodec.o
	$(CXX) $(LDFLAGS) -o getpeersdht getpeersdht.o btcodec.o
getpeersdht_r: getpeersdht_r.o btcodec.o
	$(CXX) $(LDFLAGS) -o getpeersdht_r getpeersdht_r.o btcodec.o
get_peers: get_peers.o
	$(CXX) $(LDFLAGS) -o get_peers get_peers.o

clean:
	rm -f *.o

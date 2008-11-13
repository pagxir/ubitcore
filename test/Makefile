all: pingdht finddht getpeersdht

pingdht: pingdht.o btcodec.o
	$(CXX) $(LDFLAGS) -o pingdht pingdht.o btcodec.o
finddht: finddht.o btcodec.o
	$(CXX) $(LDFLAGS) -o finddht finddht.o btcodec.o
getpeersdht: getpeersdht.o btcodec.o
	$(CXX) $(LDFLAGS) -o getpeersdht getpeersdht.o btcodec.o

clean:
	rm -f *.o

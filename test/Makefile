all: pingdht finddht

pingdht: pingdht.o btcodec.o
	$(CXX) $(LDFLAGS) -o pingdht pingdht.o btcodec.o
finddht: finddht.o btcodec.o
	$(CXX) $(LDFLAGS) -o finddht finddht.o btcodec.o

clean:
	rm -f *.o

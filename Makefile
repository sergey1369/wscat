# Makefile

CXXFLAGS=-Wall -g
LIBS=-lPocoFoundation -lPocoNet -lPocoNetSSL -lPocoUtil

wscat: wscat.cpp
	$(CXX) $(CXXFLAGS) -o wscat wscat.cpp $(LIBS)

clean:
	rm -f wscat

run: wscat
	./wscat http://localhost:8000

gdb: wscat
	gdb wscat -ex 'r http://localhost:8000'

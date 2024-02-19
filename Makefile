# Makefile

CXXFLAGS=-Wall -g
LDLIBS=-lPocoFoundation -lPocoNet -lPocoNetSSL -lPocoUtil

all: wscat wscatd

clean:
	rm -f wscat wscatd

run: wscat wscatd
	./wscatd 8000 22
	./wscat ws://localhost:8000/
	pkill ./wscatd

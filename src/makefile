CC=g++
CFLAGS=-c -std=c++11 -pthread
LDFLAGS=-std=c++11 -pthread
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=xlxd

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o

install:
	mkdir -p /xlxd
	mv /xlxd/xlxd.blacklist /xlxd/xlxd.blacklist.bak
	mv /xlxd/xlxd.whitelist /xlxd/xlxd.whitelist.bak
	mv /xlxd/xlxd.interlink /xlxd/xlxd.interlink.bak
	cp ./xlxd /xlxd/
	cp ../config/xlxd.blacklist /xlxd/
	cp ../config/xlxd.whitelist /xlxd/
	cp ../config/xlxd.interlink /xlxd/

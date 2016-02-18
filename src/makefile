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
	cp ./xlxd /xlxd/
	cp -i ../config/xlxd.blacklist /xlxd/
	cp -i ../config/xlxd.whitelist /xlxd/
	cp -i ../config/xlxd.interlink /xlxd/

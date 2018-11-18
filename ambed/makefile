CXX=g++
CFLAGS=-Os -Wall -std=c++11 -pthread
LDFLAGS=-Os -Wall -std=c++11 -pthread 
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=ambed

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	 $(CXX) $(LDFLAGS) $(OBJECTS) -lftd2xx -Wl,-rpath,/usr/local/lib -o $@

.cpp.o:
	 $(CXX) $(CFLAGS) -c -o $@ $<

clean:
	 $(RM) xlxd *.o *.d *.bak

install:
	 mkdir -p /ambed
	 cp $(EXECUTABLE) /ambed/

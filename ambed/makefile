CC=g++
CFLAGS=-c -std=c++11 -pthread
LDFLAGS=-std=c++11 -pthread 
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=ambed

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -lftd2xx -Wl,-rpath,/usr/local/lib -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) *.o

install:
	mkdir -p /ambed
	cp ./ambed /ambed/
	cp ./run /ambed/

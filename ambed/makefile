CC=g++
CFLAGS=-c -std=c++11 -pthread
LDFLAGS=-std=c++11 -pthread
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
CODEC2_SOURCES=$(wildcard codec2/*.cpp)
CODEC2_OBJECTS=$(CODEC2_SOURCES:.cpp=.o)
IMBE_SOURCES=$(wildcard imbe/*.cc)
IMBE_OBJECTS=$(IMBE_SOURCES:.cc=.o)
EXECUTABLE=ambed

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) $(CODEC2_OBJECTS) $(IMBE_OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(CODEC2_OBJECTS) $(IMBE_OBJECTS) -lftd2xx -Wl,-rpath,/usr/local/lib -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

codec2/%.o: codec2/%.cpp
	$(CC) $(CFLAGS) $< -o $@

imbe/%.o: imbe/%.cc
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) $(EXECUTABLE) *.o codec2/*.o imbe/*.o

install:
	mkdir -p /ambed
	cp $(EXECUTABLE) /ambed/
	cp ./run /ambed/

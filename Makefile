CXX=g++
CXXFLAGS=-Os -Wall -std=c++11 -pthread
LDFLAGS=-Os -Wall -std=c++11 -pthread
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=xlxd

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(EXECUTABLE) *.o *.d *.bak

install:
	mkdir -p /xlxd
	cp -f $(EXECUTABLE) /xlxd/
	[ -f /xlxd/xlxd.blacklist ] && \
		cp ../config/xlxd.blacklist /xlxd/xlxd.blacklist.sample || \
		cp ../config/xlxd.blacklist /xlxd/xlxd.blacklist
	[ -f /xlxd/xlxd.whitelist ] && \
		cp ../config/xlxd.whitelist /xlxd/xlxd.whitelist.sample || \
		cp ../config/xlxd.whitelist /xlxd/xlxd.whitelist
	[ -f /xlxd/xlxd.interlink ] && \
		cp ../config/xlxd.interlink /xlxd/xlxd.interlink.sample || \
		cp ../config/xlxd.interlink /xlxd/xlxd.interlink

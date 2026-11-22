ALL_CXXFLAGS=-std=c++11 -pthread $(CXXFLAGS)
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=xlxd

# variables for staged installation
# directory variables - https://www.gnu.org/software/make/manual/html_node/Directory-Variables.html
# DESTDIR - https://www.gnu.org/software/make/manual/html_node/DESTDIR.html
# ComamndVariables - #https://www.gnu.org/software/make/manual/html_node/Command-Variables.html
prefix=/usr/local
exec_prefix=$(prefix)
# by default install under /xlxd/ - it should be $(exec_dir)/sbin
sbin_dir=/xlxd
# by default install config under /xlxd/ - it should be $(prefix)/etc
sysconf_dir=/xlxd
INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=${INSTALL} -m 644

all: $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(ALL_CXXFLAGS) $(OBJECTS) -o $@

.cpp.o: %.h
	$(CXX) -c $(CPPFLAGS) $(ALL_CXXFLAGS) $<

.PHONY: clean
clean:
	$(RM) $(EXECUTABLE) *.o

.PHONY: install
install:
	$(INSTALL) -d $(DESTDIR)$(sbin_dir)
	$(INSTALL_PROGRAM) $(EXECUTABLE) $(DESTDIR)$(sbin_dir)/$(EXECUTABLE)
	$(INSTALL) -d $(DESTDIR)$(sysconf_dir)
	[ -f $(DESTDIR)$(sysconf_dir)/xlxd.blacklist ] && \
		$(INSTALL_DATA) ../config/xlxd.blacklist  $(DESTDIR)$(sysconf_dir)/xlxd.blacklist.sample || \
		$(INSTALL_DATA) ../config/xlxd.blacklist  $(DESTDIR)$(sysconf_dir)/xlxd.blacklist
	[ -f $(DESTDIR)$(sysconf_dir)/xlxd.whitelist ] && \
		$(INSTALL_DATA) ../config/xlxd.whitelist  $(DESTDIR)$(sysconf_dir)/xlxd.whitelist.sample || \
		$(INSTALL_DATA) ../config/xlxd.whitelist  $(DESTDIR)$(sysconf_dir)/xlxd.whitelist
	[ -f $(DESTDIR)$(sysconf_dir)/xlxd.interlink ] && \
		$(INSTALL_DATA) ../config/xlxd.interlink  $(DESTDIR)$(sysconf_dir)/xlxd.interlink.sample || \
		$(INSTALL_DATA) ../config/xlxd.interlink  $(DESTDIR)$(sysconf_dir)/xlxd.interlink
	[ -f $(DESTDIR)$(sysconf_dir)/xlxd.terminal ] && \
		$(INSTALL_DATA) ../config/xlxd.terminal  $(DESTDIR)$(sysconf_dir)/xlxd.terminal.sample || \
		$(INSTALL_DATA) ../config/xlxd.terminal  $(DESTDIR)$(sysconf_dir)/xlxd.terminal

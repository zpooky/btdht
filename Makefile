# Makefile
#http://www.puxan.com/web/howto-write-generic-makefiles/
# Declaration of variables
CXX = g++
HEADER_DIRS = -Iexternal -Iexternal/sputil/include -Isrc
# ovrrides makes it possible to externaly append extra flags
override CXXFLAGS += $(HEADER_DIRS) -enable-frame-pointers -std=c++17 -Wall -Wextra -Wpedantic -Wpointer-arith -Wconversion -Wshadow
CXXFLAGS_DEBUG = $(CXXFLAGS) -ggdb -O0
LDFLAGS = -fno-omit-frame-pointer
LDLIBS = -Lexternal/sputil/build/dht -lsputil -lbfd -ldl
PREFIX = /usr/local
BUILD_DIR = build/debug

# File names
EXEC = dht
EXEC_CLIENT = dht-client

LIB = lib$(EXEC)

SOURCES = $(wildcard src/*.cpp)
OBJECTS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SOURCES))

OBJECTS_SERVER = $(OBJECTS)
OBJECTS_SERVER += $(BUILD_DIR)/dht-server.o

OBJECTS_CLIENT = $(OBJECTS)
OBJECTS_CLIENT += $(BUILD_DIR)/dht-client.o

# TODO fix proper dependency (mssing client&server)
DEPENDS = $(OBJECTS:.o=.d)

# all {{{
# The "all" target. runs by default since it the first target
.PHONY: all
all: $(EXEC) $(EXEC_CLIENT)
	$(AR) rcs $(BUILD_DIR)/$(LIB).a $(OBJECTS_SERVER)
# }}}

# $(EXEC) {{{
# depends on the targets for all the object files
$(EXEC): $(OBJECTS_SERVER) dependencies
	$(CXX) $(OBJECTS_SERVER) -o $(EXEC) $(LDLIBS)
# }}}

$(EXEC_CLIENT): $(OBJECTS_CLIENT) dependencies
	$(CXX) $(OBJECTS_CLIENT) -o $(EXEC_CLIENT) $(LDLIBS)

# object {{{

#include the raw object file dependencies from its *.d file
-include $(DEPENDS)

# The "object" file target
# An implicit conversion from a cpp file to a object file?
$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(dir $(@))
# -c means to create an intermediary object file, rather than an executable
# -MMD means to create *object*.d depend file with its depending cpp & h files
	$(CXX) $(CXXFLAGS_DEBUG) $(LDFLAGS) -MMD -c $< -o $@
# }}}

# clean {{{
.PHONY: clean
clean:
	rm -f $(OBJECTS)
	rm -f $(DEPENDS)
	rm -f $(EXEC)
	rm -f $(EXEC_CLIENT)
	rm -f $(BUILD_DIR)/$(LIB).a $(BUILD_DIR)/$(LIB).so
	$(MAKE) -C test clean
	$(MAKE) -C external/sputil BUILD_DIR=build/dht clean
# }}}

# test {{{
.PHONY: test
test:
	$(MAKE) -C test test
# }}}

# install {{{
.PHONY: install
install: $(EXEC) staticlib
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/lib
# mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp -f $(EXEC) $(DESTDIR)$(PREFIX)/bin
	cp -f $(BUILD_DIR)/$(LIB).a $(DESTDIR)$(PREFIX)/lib
# gzip < $(EXEC).1 > $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1.gz
# }}}

# uninstall {{{
.PHONY: uninstall
uninstall:
	rm $(DESTDIR)$(PREFIX)/bin/$(EXEC)
	rm $(DESTDIR)$(PREFIX)/bin/$(lib).a
# rm $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1.gz
# }}}

# bear {{{
# Creates compilation_database.json
.PHONY: bear
bear:
	$(MAKE) -C test bear
	$(MAKE) -C client bear
	$(MAKE) -C external/sputil bear
	make BUILD_DIR=build/bear clean
	bear make BUILD_DIR=build/bear CXXFLAGS+=-DSP_TEST -j
	compdb list > tmp_compile_commands.json
	mv tmp_compile_commands.json compile_commands.json
# }}}

.PHONY: dependencies
dependencies:
	$(MAKE) -C external/sputil BUILD_DIR=build/dht debug

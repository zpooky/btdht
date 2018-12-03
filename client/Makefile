# Makefile
#http://www.puxan.com/web/howto-write-generic-makefiles/
# Declaration of variables
CXX = g++
HEADER_DIRS = -I.. -I../external/sputil/include
# ovrrides makes it possible to externaly append extra flags
override CXXFLAGS += $(HEADER_DIRS) -enable-frame-pointers -std=c++17 -Wall -Wextra -Wpedantic -Wpointer-arith -Wconversion -Wshadow
CXXFLAGS_DEBUG = $(CXXFLAGS) -ggdb
LDFLAGS =
LDLIBS = -L../build/client -ldht -L../external/sputil/build/client -lsputil -lbfd -ldl
PREFIX = /usr/local

SOURCE_DIR = src
BUILD_DIR = build/debug

# File names
EXEC = dht-client
LIB = lib$(EXEC)
SOURCES = $(wildcard $(SOURCE_DIR)/*.cpp)
OBJECTS = $(patsubst $(SOURCE_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)

# PHONY targets is not file backed targets
.PHONY: all clean install uninstall bear

# all {{{
# The "all" target. runs by default since it the first target
all: ${EXEC}
	$(AR) rcs $(BUILD_DIR)/$(LIB).a $(OBJECTS)
# }}}

# $(EXEC) {{{
# depends on the targets for all the object files
$(EXEC): dependencies $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(EXEC) $(LDLIBS)
# }}}

# object {{{

#include the raw object file dependencies from its *.d file
-include $(DEPENDS)

# The "object" file target
# An conversion from a cpp file to a object file?
$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	mkdir -p $(BUILD_DIR)
# -c means to create an intermediary object file, rather than an executable
# -MMD means to create *object*.d depend file with its depending cpp & h files
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -MMD -c $< -o $@
# }}}

# clean {{{
clean:
	rm -f $(OBJECTS)
	rm -f $(DEPENDS)
	rm -f $(EXEC) $(BUILD_DIR)/$(LIB).a $(BUILD_DIR)/$(LIB).so
	$(MAKE) -C .. clean
# }}}

# install {{{
install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/lib
# mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp -f $(EXEC) $(DESTDIR)$(PREFIX)/bin
	cp -f $(BUILD_DIR)/$(LIB).a $(DESTDIR)$(PREFIX)/lib
# gzip < $(EXEC).1 > $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1.gz
# }}}

# uninstall {{{
uninstall:
	rm $(DESTDIR)$(PREFIX)/bin/$(EXEC)
	rm $(DESTDIR)$(PREFIX)/bin/$(lib).a
# rm $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1.gz
# }}}

# bear {{{
# Creates compilation_database.json
bear:
	make BUILD_DIR=build/bear clean
	bear make BUILD_DIR=build/bear CXXFLAGS+=-DSP_TEST -j
	compdb list > tmp_compile_commands.json
	mv tmp_compile_commands.json compile_commands.json
# }}}

dependencies:
	$(MAKE) -C .. BUILD_DIR=build/client
	$(MAKE) -C ../external/sputil BUILD_DIR=build/client

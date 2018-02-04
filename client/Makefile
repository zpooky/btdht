# Makefile
#http://www.puxan.com/web/howto-write-generic-makefiles/
# Declaration of variables
CXX = g++
HEADER_DIRS = -I.. -I../external/sputil/include
# ovrrides makes it possible to externaly append extra flags
override CXXFLAGS += $(HEADER_DIRS) -enable-frame-pointers -std=c++14 -Wall -Wextra -Wpedantic -Wpointer-arith -Wconversion -Wshadow
CXXFLAGS_DEBUG = $(CXXFLAGS) -ggdb
LDFLAGS =
LDLIBS = -L../build -ldht -L../external/sputil/build -lsputil
PREFIX = /usr/local

SOURCE_DIR = src
BUILD_DIR = build

# File names
EXEC = dht-client
LIB = lib$(EXEC)
SOURCES = $(wildcard $(SOURCE_DIR)/*.cpp)
OBJECTS = $(patsubst $(SOURCE_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)

#TODO dynamic link lib
#TODO https://kristerw.blogspot.se/2017/09/useful-gcc-warning-options-not-enabled.html
#TODO build for different optimizations level in dedicated build directories

# PHONY targets is not file backed targets
.PHONY: all clean install uninstall bear

# all {{{
# The "all" target. runs by default since it the first target
all: ${EXEC}
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

# staticlib {{{
staticlib: $(OBJECTS)
# 'r' means to insert with replacement
# 'c' means to create a new archive
# 's' means to write an index
	$(AR) rcs $(BUILD_DIR)/$(LIB).a $(OBJECTS)
# }}}

# install {{{
install: $(EXEC) staticlib
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
bear: clean
	bear make
# }}}

dependencies:
	$(MAKE) -C .. staticlib

# gcc
##linker flags
# -static - On systems that support dynamic linking, this prevents linking with the shared libraries.

# Make
## Rule
# target: dependencies

##ref
# -http://nullprogram.com/blog/2017/08/20/
# -https://swcarpentry.github.io/make-novice/reference/

## Special Macros
#$@ - The target of the current rule.

#$* - The target with the suffix cut off
# example: $* of prog.c would be prog

#$< - the name of the related file that caused the action
# The name of the file that caused this target to get triggered and made. If we are
# making prog.o, it is probably because prog.c has recently been modified, so $< is
# prog.c.

# $? - is the names of the changed dependents.

# $^ - The dependencies of the current rule.

## Externally override flags
# make CC=clang CFLAGS='-O3 -march=native'

## Read flags from env
#export CC=clang
#export CFLAGS=-O3
#make -e all

## Generate default make values explicitly
#make -p

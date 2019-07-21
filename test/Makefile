# Makefile
# Declaration of variables
CXX = g++
# using -std=gnu++14 because gtest uses functions only presisent in the GNU dialect of c++
PROJECT_ROOT = ..
GTEST_ROOT = $(PROJECT_ROOT)/external/googletest/googletest
HEADER_DIRS = -I$(GTEST_ROOT)/include -I$(PROJECT_ROOT) -I$(PROJECT_ROOT)/src -I$(PROJECT_ROOT)/external/sputil/include
override CXXFLAGS += $(HEADER_DIRS) -enable-frame-pointers -std=gnu++14 -Wall -Wextra -Wpointer-arith -ggdb
SUT_DIR = build/test

LDLIBS = -L$(GTEST_ROOT)/make -lgtest_main -lpthread -L$(PROJECT_ROOT)/$(SUT_DIR) -lsut -L$(PROJECT_ROOT)/external/sputil/build/dht -lsputil
ifeq ($(OS),Windows_NT)
else
	LDLIBS += -lbfd -ldl
endif

BUILD = build

# TODO `make all` get relinks to gtest because I copy gtest_main to lib... and change time stamp?
# TODO coverage
# TODO sut external headers?

# File names
EXEC = thetest
SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp, $(BUILD)/%.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)

.PHONY: test all clean gtest

# all {{{
all: $(EXEC)
# }}}

# $(EXEC) linking {{{
$(EXEC): sut gtest $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(EXEC) $(LDLIBS)
# }}}

# compile {{{
-include $(DEPENDS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -MMD -c $< -o $@
# }}}

# clean {{{
clean:
# clean test binaries
	rm -f $(EXEC) $(OBJECTS) $(DEPENDS)
# Clean sut binaries
	rm -f $(PROJECT_ROOT)/$(SUT_DIR)/*.a
	rm -f $(PROJECT_ROOT)/$(SUT_DIR)/*.d
	rm -f $(PROJECT_ROOT)/$(SUT_DIR)/*.o
# }}}

# gtest {{{
LIBGTEST_ROOT = $(GTEST_ROOT)/make
gtest:
	$(MAKE) -C $(GTEST_ROOT)/make all
	cp $(LIBGTEST_ROOT)/gtest_main.a $(LIBGTEST_ROOT)/libgtest_main.a
# }}}

# sut {{{
sut:
# Build sut binaries and put result in $(SUT_DIR) directory to differentiate the build for test binaries with regular binaries.
# Define the SP_TEST preprocessor macro
# The resulting static library will be called libsut
	$(MAKE) BUILD_DIR=$(SUT_DIR) LIB=libsut CXXFLAGS+=-DSP_TEST -C $(PROJECT_ROOT)
# }}}

# test {{{
test: all
	./$(EXEC)
# }}}

# bear {{{
# Creates compilation_database.json
bear:
	make BUILD_DIR=build/bear clean
	bear make BUILD_DIR=build/bear CXXFLAGS+=-DSP_TEST -j
	compdb list > tmp_compile_commands.json
	mv tmp_compile_commands.json compile_commands.json
# }}}

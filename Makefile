SHELL = /bin/sh

# ------------------------------------------------------------------
#  List of all programs to be build.
#  Each program must have a corresponding <name>.c source file.
# ------------------------------------------------------------------
PROGRAMS := bad_scheduling_no_depend finegrained_single_creator finegrained_single_creator_dep finegrained_multi_creator finegrained_multi_creator_dep mixedgrained_multi_creator coarsegrained_single_creator independent_uniform_dep_chains independent_uniform_create_chains independent_uniform_taskwait_chains bad_scheduling_dep_chain tree_dep_chains tree_create_chains tree_taskwait_chains

# -------------------------------------------------------------------
# Where to find the source files and where to put the executables
# -------------------------------------------------------------------
SRC_DIR := src
BIN_DIR := bin

vpath %.c   $(SRC_DIR)
vpath %.cpp $(SRC_DIR)
vpath %.h   $(SRC_DIR)

BIN_PROGS := $(addprefix $(BIN_DIR)/,$(PROGRAMS))

# ------------------------------------------------------------------
#  Compiler and linker flags
# ------------------------------------------------------------------
CC      ?= icx
CXX     ?= icpx
CFLAGS  ?= -qopenmp -Wall -Wextra -I$(SRC_DIR)
LDFLAGS ?= -qopenmp

# ------------------------------------------------------------------
#  Default target – build *all* programs and clean up all object files
# ------------------------------------------------------------------
.PHONY: all
all: $(BIN_PROGS) cleanobjs

cleanobjs:
	@rm -f $(BIN_DIR)/*.o


# ------------------------------------------------------------------
#  Build pattern rules
# ------------------------------------------------------------------
$(BIN_PROGS): %: %.o $(BIN_DIR)/delay.o $(BIN_DIR)/parse_flags.o $(BIN_DIR)/debug.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(LDFLAGS) $^ -o $@

$(BIN_DIR)/%.o: %.c delay.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/parse_flags.o: parse_flags.cpp parse_flags.h
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/debug.o: debug.cpp debug.h
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CFLAGS) -c $< -o $@


# ------------------------------------------------------------------
#   Clean up
# ------------------------------------------------------------------
.PHONY: clean
clean:
	rm -f $(BIN_PROGS)

.PHONY: cleanall
cleanall: clean
	rm -f $(BIN_DIR)/*.o
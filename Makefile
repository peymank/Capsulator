# Makefile for Capsulator
# ------------------------------------------------------------------------------
# make        -- builds Capsulator and all dependencies in the default mode
# make debug  -- builds Capsulator in debug mode
# make release-- builds Capsulator in release mode
# make clean  -- clean up byproducts

# utility programs used by this Makefile
CC   = gcc
MAKE = gmake --no-print-directory

# set system-dependent variables
OSTYPE = $(shell uname)
ifeq ($(OSTYPE),Linux)
ARCH=-D_LINUX_
ENDIAN=-D_LITTLE_ENDIAN_
LIB_SOCKETS =
endif
ifeq ($(OSTYPE),SunOS)
ARCH=-D_SOLARIS_
ENDIAN=-D_BIG_ENDIAN_
LIB_SOCKETS = -lnsl -lsocket
endif

# define names of our build targets
APP = capsulator

# compiler and its directives
DIR_INC       =
DIR_LIB       =
LIBS          = $(LIB_SOCKETS) -lpthread
FLAGS_CC_BASE = -c -Wall $(ARCH) $(ENDIAN) $(DIR_INC)

# compiler directives for debug and release modes
BUILD_TYPE = debug
ifeq ($(BUILD_TYPE),debug)
FLAGS_CC_BUILD_TYPE = -g -D_DEBUG_
else
FLAGS_CC_BUILD_TYPE = -O3
endif

# put all the flags together
CFLAGS = $(FLAGS_CC_BASE) $(FLAGS_CC_BUILD_TYPE)

# project sources
SRCS = common.c capsulator.c get_ip_for_interface.c main.c
OBJS = $(patsubst %.c,%.o,$(SRCS))
DEPS = $(patsubst %.c,.%.d,$(SRCS))

# include the dependencies once we've built them
ifdef INCLUDE_DEPS
include $(DEPS)
endif

#########################
## PHONY TARGETS
#########################
# note targets which don't produce a file with the target's name
.PHONY: all clean clean-all clean-deps debug release deps

# build the program
all: $(APP)

# clean up by-products (except dependency files)
clean:
	rm -f *.o $(APP)

# clean up all by-products
clean-all: clean clean-deps

# clean up dependency files
clean-deps:
	rm -f .*.d

# shorthand for building debug or release builds
debug release:
	@$(MAKE) BUILD_TYPE=$@ all

# build the dependency files
deps: $(DEPS)

# includes are ready build command
IR=ir
$(APP).$(IR): $(OBJS)
	$(CC) -o $(APP) $(OBJS) $(DIR_LIB) $(LIBS)

#########################
## REAL TARGETS
#########################
$(APP): deps
	@$(MAKE) BUILD_TYPE=$(BUILD_TYPE) INCLUDE_DEPS=1 $@.$(IR)

$(DEPS): .%.d: %.c
	$(CC) -MM $(CFLAGS) $(DIRS_INC) $< > $@

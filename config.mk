ifndef PREFIX
    PREFIX := /usr/local
endif
ifndef LIBDIR
    LIBDIR := $(PREFIX)/lib
endif

CC = $(shell which gcc)
AR = $(shell which ar)
LD = $(shell which ld)

## Common flags
FLG    = -std=gnu11
SFLAGS = -O3 -Wall -Werror -Wno-error=pointer-sign -fPIC
ZLIB   = -DZLIB_API
ZLIB_PATH ?= libz.so

## Compiler related flags
GCC_VER = $(shell $(CC) -dumpversion)
GCC_VER_MAIN = $(shell echo $(GCC_VER) | cut -d'.' -f1)

ifeq "$(shell expr $(GCC_VER_MAIN) \>= 6)" "1"
    SFLAGS += -mcpu=power9
endif

CFLAGS = $(FLG) $(SFLAGS) $(ZLIB) -DZLIB_PATH=\"$(ZLIB_PATH)\"

CC = $(shell which gcc)
AR = $(shell which ar)
LD = $(shell which ld)

## Common flags
FLG    = -std=gnu11
SFLAGS = -O3 -fPIC -D_LARGEFILE64_SOURCE=1 -DHAVE_HIDDEN
ZLIB   = -DZLIB_API

## Compiler related flags
GCC_VER = $(shell $(CC) -dumpversion)
GCC_VER_MAIN = $(shell echo $(GCC_VER) | cut -d'.' -f1)

ifeq "$(shell expr $(GCC_VER_MAIN) \>= 6)" "1"
    SFLAGS += -mcpu=power9
endif

CFLAGS = $(FLG) $(SFLAGS) $(ZLIB) #-DNXTIMER



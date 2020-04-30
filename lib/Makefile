include ../config.mk

SRCS = nx_inflate.c nx_deflate.c nx_zlib.c nx_crc.c nx_dht.c nx_dhtgen.c nx_dht_builtin.c \
       nx_adler32.c gzip_vas.c nx_compress.c nx_uncompr.c crc32_ppc.c crc32_ppc_asm.S nx_utils.c
OBJS = nx_inflate.o nx_deflate.o nx_zlib.o nx_crc.o nx_dht.o nx_dhtgen.o nx_dht_builtin.o \
       nx_adler32.o gzip_vas.o nx_compress.o nx_uncompr.o crc32_ppc.o crc32_ppc_asm.o nx_utils.o

ZLIB_API_SRCS =
ZLIB_API_OBJS =
ZLIB_API_OBJ =

ifeq ($(ZLIB_API),YES)
  ZLIB_API_SRCS = nx_zlib_api_general.c nx_zlib_api_crc32.c \
		  nx_zlib_api_adler32.c nx_zlib_api_uncompr.c
  ZLIB_API_OBJS = nx_zlib_api_general.o nx_zlib_api_crc32.o \
		  nx_zlib_api_adler32.o nx_zlib_api_uncompr.o
  ZLIB_API_OBJ = nx_zlib_api.o
endif

VERSION ?= $(shell git describe --tags | cut -d - -f 1,2 | tr - . | cut -c 2-)
SOVERSION = $(shell echo $(VERSION) | cut -d . -f 1)
PACKAGENAME = libnxz
STATICLIB = $(PACKAGENAME).a
SHAREDSONAMELIB = $(PACKAGENAME).so.$(SOVERSION)
SHAREDLIB = $(PACKAGENAME).so.$(VERSION)
LIBLINK = $(PACKAGENAME).so

INC = ../inc_nx

all: $(OBJS) $(STATICLIB) $(SHAREDLIB)

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) -I$(INC) -c $^

$(ZLIB_API_OBJS): $(ZLIB_API_SRCS)
	$(CC) $(CFLAGS) -I$(INC) -c $^

$(ZLIB_API_OBJ): $(ZLIB_API_OBJS)
	$(LD) -relocatable -o $@ $^

$(STATICLIB): $(OBJS) $(ZLIB_API_OBJ)
	rm -f $@
	$(AR) rcs -o $@ $^

$(SHAREDLIB): $(OBJS) $(ZLIB_API_OBJ)
	rm -f $@ $(LIBLINK) $(SHAREDSONAMELIB)
	$(CC) -shared  -Wl,-soname,$(SHAREDSONAMELIB),--version-script,Versions -o $@ $^
	ln -s $@ $(LIBLINK)
	ln -s $@ $(SHAREDSONAMELIB)

clean:
	rm -f *.o *.gcda *.gcno *.so* *.a *~


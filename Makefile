OPTCC = /opt/at11.0/bin/gcc
ifneq ("$(wildcard $(OPTCC))","")
	CC = $(OPTCC)
else
	CC = gcc
endif
FLG = -std=gnu11
SFLAGS = -O3 -fPIC -D_LARGEFILE64_SOURCE=1 -DHAVE_HIDDEN
ZLIB = -DZLIB_API
CFLAGS = $(FLG) $(SFLAGS) $(ZLIB) -mcpu=power9 #-DNXTIMER

SRCS = nx_inflate.c nx_deflate.c nx_zlib.c nx_crc.c nx_dht.c nx_dhtgen.c nx_dht_builtin.c \
       nx_adler32.c gzip_vas.c nx_compress.c nx_uncompr.c crc32_ppc.c crc32_ppc_asm.S nx_utils.c
OBJS = nx_inflate.o nx_deflate.o nx_zlib.o nx_crc.o nx_dht.o nx_dhtgen.o nx_dht_builtin.o \
       nx_adler32.o gzip_vas.o nx_compress.o nx_uncompr.o crc32_ppc.o crc32_ppc_asm.o nx_utils.o

VERSION ?= $(shell git describe --tags | cut -d - -f 1,2 | tr - . | cut -c 2-)
STATICLIB = libnxz.a
SHAREDLIB = libnxz.so.$(VERSION)
LIBLINK = libnxz.so

INC = ./inc_nx

all: $(OBJS) $(STATICLIB) $(SHAREDLIB)

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) -I$(INC) -c $^

$(STATICLIB): $(OBJS)
	rm -f $@
	ar rcs -o $@ $(OBJS)

$(SHAREDLIB): $(OBJS)
	rm -f $@ $(LIBLINK) 
	$(CC) -shared  -Wl,-soname,libnxz.so,--version-script,zlib.map -o $@ $(OBJS)
	ln -s $@ $(LIBLINK)

clean:
	/bin/rm -f *.o *.gcda *.gcno *.so *.a *~ $(SHAREDLIB)
	$(MAKE) -C test $@

check:
	$(MAKE) -C test $@

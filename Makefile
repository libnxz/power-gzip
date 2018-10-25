# CC=gcc
CC = /opt/at11.0/bin/gcc
FLG = -std=gnu11
SFLAGS = -O3 -fPIC -D_LARGEFILE64_SOURCE=1 -DHAVE_HIDDEN
# ZLIB = -DZLIB_API
CFLAGS = $(FLG) $(SFLAGS) $(ZLIB)

SRCS = nx_inflate.c nx_deflate.c nx_zlib.c nx_crc.c \
       nx_adler_combine.c gzip_vas.c nx_compress.c nx_uncompr.c
OBJS = nx_inflate.o nx_deflate.o nx_zlib.o nx_crc.o \
       nx_adler_combine.o gzip_vas.o nx_compress.o nx_uncompr.o

STATICLIB = libnxz.a
SHAREDLIB = libnxz.so

INC = ./inc_nx

all: $(OBJS) $(STATICLIB) $(SHAREDLIB)

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) -I$(INC) -c $^

$(STATICLIB): $(OBJS)
	rm -f $@
	ar rcs -o $@ $(OBJS)

$(SHAREDLIB): $(OBJS)
	rm -f $@
	$(CC) -shared  -Wl,-soname,libnxz.so,--version-script,zlib.map -o $@ $(OBJS)	

clean:
	/bin/rm -f *.o *.gcda *.gcno *.so *.a


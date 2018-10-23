# CC=gcc
CC=/opt/at11.0/bin/gcc
FLG= -std=gnu11
SFLAGS=-O3 -fPIC -D_LARGEFILE64_SOURCE=1 -DHAVE_HIDDEN
CFLAGS=$(FLG) $(SFLAGS)

SRCS = nx_inflate.c nx_deflate.c nx_zlib.c nx_crc.c nx_adler_combine.c gzip_vas.c
OBJS = nx_inflate.o nx_deflate.o nx_zlib.o nx_crc.o nx_adler_combine.o gzip_vas.o
STATICLIB = libnxz.a
SHAREDLIB = libnxz.so

ZDIR   = ../p9z/zlib-master
ZLIB   = $(ZDIR)/libz.so.1.2.11
ZCONFDIR = CONFIG_ZLIB_PATH="\"$(ZLIB)\""

INC = ./inc_nx

all: $(OBJS) $(STATICLIB) $(SHAREDLIB)

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) -I$(INC) -I$(ZDIR) -c $^

$(STATICLIB): $(OBJS)
	rm -f $@
	ar rcs -o $@ $(OBJS)

$(SHAREDLIB): $(OBJS)
	rm -f $@
	$(CC) -shared  -Wl,-soname,libnxz.so.1,--version-script,zlib.map -o $@ $(OBJS)	

clean:
	/bin/rm -f *.o *.gcda *.gcno *.so *.a


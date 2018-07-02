CC=gcc
FLG=
CFLAGS=-O3 $(FLG)
LIBDIR = ../nxu_lib
LIBOBJ =
LIBSRC = $(LIBOBJ:.o=.c)
ZDIR   = ../p9z/zlib-master
ZLIB   = $(ZDIR)/libz.so.1.2.11
ZCONFDIR = CONFIG_ZLIB_PATH="\"$(ZLIB)\""
INC = ./inc_nx

all: 	nx_zlib_test

nx_zlib_test: nx_zlib.c gzip_vas.c nx_adler_combine.c nx_crc.c nx_inflate.c
	$(CC) $(CFLAGS) -I$(INC) -I$(ZDIR) -D_NX_ZLIB_TEST -o $@ $^

clean:
	/bin/rm -f *.o *~ nx_zlib_test *.gcda *.gcno


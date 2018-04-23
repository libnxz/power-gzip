CC=gcc
FLG=
CFLAGS=-O3 $(FLG)
LIBDIR = ../nxu_lib
LIBOBJ =
LIBSRC = $(LIBOBJ:.o=.c)
ZDIR   = ../zlib-master
ZLIB   = $(ZDIR)/libz.so.1.2.11
ZCONFDIR = CONFIG_ZLIB_PATH="\"$(ZLIB)\""
INC = ./inc_nx

all: 	nx_zlib_test

nx_zlib_test: nx_zlib.c gzip_vas.c nx_adler_combine.c nx_crc.c
	$(CC) $(CFLAGS) -I$(INC) -I$(ZDIR) -D_NX_GZIP_TEST -o $@ $^

clean:
	/bin/rm -f *.o *~ nxu_sw_test nxu_hw_test *.gcda *.gcno


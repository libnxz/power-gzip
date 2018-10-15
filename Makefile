CC=gcc
FLG= -std=gnu11
CFLAGS=-O3 $(FLG)

LIBOBJ = libnxz.so
SRCS = nx_inflate.c nx_zlib.c nx_crc.c nx_adler_combine.c gzip_vas.c
OBJS = nx_inflate.o nx_zlib.o nx_crc.o nx_adler_combine.o gzip_vas.o

ZDIR   = ../p9z/zlib-master
ZLIB   = $(ZDIR)/libz.so.1.2.11
ZCONFDIR = CONFIG_ZLIB_PATH="\"$(ZLIB)\""

INC = ./inc_nx
#NXFLAGS = -DNXDBG

all: nx_zlib_test $(OBJS) $(LIBOBJ)

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -I$(ZDIR) -c $^

$(LIBOBJ): $(OBJS)
	rm -f $@
	$(CC) -shared -o $@ $(OBJS)	

nx_zlib_test: nx_zlib.c gzip_vas.c nx_adler_combine.c nx_crc.c nx_inflate.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -I$(ZDIR) -D_NX_ZLIB_TEST -o $@ $^ -lz

clean:
	/bin/rm -f *.o *~ nx_zlib_test *.gcda *.gcno *.so


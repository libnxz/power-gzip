
include ../config.mk

SRC =
INC = ../inc_nx
LDLIB = ../lib/libnxz.a -lpthread
TESTS = gunzip_nx_test gzip_nxfht_test gzip_nxdht_test compdecomp_th
NXFLAGS = #-DNXDBG  #-DNXDBG -DNXTIMER -DNX_MMAP
DHT_O = gzip_nxdht.o gzip_vas.o nx_dht.o nx_dht_builtin.o nx_dhtgen.o
FHT_O = gzip_nxfht.o gzip_vas.o
GUN_O = gunzip_nx.o gzip_vas.o

all:

bench: $(TESTS) zpipe gzm nx_gzip

gzip_vas.o:	../lib/gzip_vas.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../lib/gzip_vas.c

gzip_nxfht.o:	gzip_nxfht.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxfht.c

nx_dht.o:	../lib/nx_dht.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../lib/nx_dht.c

nx_dht_builtin.o:	../lib/nx_dht_builtin.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../lib/nx_dht_builtin.c

nx_dhtgen.o:	../lib/nx_dhtgen.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../lib/nx_dhtgen.c

nx_dht_decomp.o:	../lib/nx_dht_decomp.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../lib/nx_dht_decomp.c

#define SAVE_LZCOUNTS only when printing dhts in the cache
gzip_nxdht.o:	gzip_nxdht.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxdht.c #-DSAVE_LZCOUNTS

gunzip_nx.o:	gunzip_nx.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c gunzip_nx.c

gzip_nxfht_test:	$(FHT_O)
	$(CC) $(CFLAGS) -o gzip_nxfht_test $(FHT_O)

gzip_nxdht_test:	$(DHT_O)
	$(CC) $(CFLAGS) -lm -o gzip_nxdht_test $(DHT_O)

gunzip_nx_test:		$(GUN_O)
	$(CC) $(CFLAGS) -o gunzip_nx_test $(GUN_O)

zpipe:	zpipe.c ../lib/libnxz.a
	$(CC) $(CFLAGS) -I../inc_nx -I../lib -L../ -L/usr/lib/ -o zpipe zpipe.c $(LDLIB)

zpipe_dict:  zpipe_dict.c
	$(CC) $(CFLAGS) -o zpipe_dict_zlib zpipe_dict.c -lz
	$(CC) $(CFLAGS) -I../inc_nx -I../lib -L../ -L/usr/lib/ -o zpipe_dict_nx zpipe_dict.c $(LDLIB)

crc_perf_test:  crc_perf_test.c
	$(CC) $(CFLAGS) -o crc_perf_test_zlib crc_perf_test.c -lz
	$(CC) $(CFLAGS) -o crc_perf_test_vmx  crc_perf_test.c $(LDLIB)

makedata:  makedata.c
	$(CC) $(CFLAGS) -o makedata makedata.c

gzm:	gzm.c ../lib/libnxz.a
	$(CC) $(CFLAGS) -I../inc_nx -I../lib -L../ -L/usr/lib/ -o gzm gzm.c $(LDLIB)

compdecomp:	compdecomp.c ../lib/libnxz.a
	$(CC) $(CFLAGS) -I../inc_nx -I../lib -L../ -L/usr/lib/ -o compdecomp compdecomp.c $(LDLIB)

compdecomp_th: compdecomp_th.c  ../lib/libnxz.a
	$(CC) $(CFLAGS) -I../inc_nx -I../lib -L../ -L/usr/lib/ -o compdecomp_th compdecomp_th.c $(LDLIB)

nx_gzip: nx_gzip.c
	$(CC) $(CFLAGS) -o $@ $^ -lz

clean:
	rm -f $(TESTS) *.o *.c~ *.h~ Makefile~ zpipe compdecomp compdecomp_th makedata \
	zpipe_dict_nx zpipe_dict_zlib crc_perf_test_zlib crc_perf_test_vmx gzm nx_gzip

CFLAGS = -O3
SRC =
INC = ../inc_nx
TESTS = gunzip_nx_test gzip_nxfht_test gzip_nxdht_test compdecomp_th
NXFLAGS = #-DNXDBG  #-DNXDBG -DNXTIMER -DNX_MMAP
DHT_O = gzip_nxdht.o gzip_vas.o nx_dht.o nx_dht_builtin.o nx_dhtgen.o
FHT_O = gzip_nxfht.o gzip_vas.o
GUN_O = gunzip_nx.o gzip_vas.o

all:	$(TESTS) zpipe gzm

gzip_vas.o:	../gzip_vas.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../gzip_vas.c

gzip_nxfht.o:	gzip_nxfht.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxfht.c

nx_dht.o:	../nx_dht.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../nx_dht.c

nx_dht_builtin.o:	../nx_dht_builtin.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../nx_dht_builtin.c

nx_dhtgen.o:	../nx_dhtgen.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../nx_dhtgen.c

nx_dht_decomp.o:	../nx_dht_decomp.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../nx_dht_decomp.c

#define SAVE_LZCOUNTS only when printing dhts in the cache
gzip_nxdht.o:	gzip_nxdht.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxdht.c #-DSAVE_LZCOUNTS

gunzip_nx.o:	gunzip_nx.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gunzip_nx.c

gzip_nxfht_test: 	$(FHT_O)
	gcc $(CFLAGS) -o gzip_nxfht_test $(FHT_O)

gzip_nxdht_test: 	$(DHT_O)
	gcc $(CFLAGS) -lm -o gzip_nxdht_test $(DHT_O)

#nxlite_test:	 $(DHT_O)
#	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -o nxlite_test nxlite.c gzip_vas.o nx_dht.o nx_dht_builtin.o nx_dhtgen.o

gunzip_nx_test: 	$(GUN_O)
	gcc $(CFLAGS) -o gunzip_nx_test $(GUN_O)

zpipe:	../libnxz.a zpipe.c
	gcc $(CFLAGS) -I../inc_nx -I../ -L../ -L/usr/lib/ -o zpipe zpipe.c ../libnxz.a -lpthread

gzm:	../libnxz.a gzm.c
	gcc $(CFLAGS) -I../inc_nx -I../ -L../ -L/usr/lib/ -o gzm gzm.c ../libnxz.a -lpthread

compdecomp: 	compdecomp.c ../libnxz.a
	gcc $(CFLAGS) -I../inc_nx -I../ -L../ -L/usr/lib/ -o compdecomp compdecomp.c ../libnxz.a -lpthread

compdecomp_th: compdecomp_th.c  ../libnxz.a
	gcc $(CFLAGS) -I../inc_nx -I../ -L../ -L/usr/lib/ -o compdecomp_th compdecomp_th.c ../libnxz.a -lpthread

clean:
	rm -f $(TESTS) *.o *.c~ *.h~ Makefile~ zpipe compdecomp compdecomp_th nxlite_test 

CFLAGS = -O3
SRC =
INC = ../inc_nx
TESTS = gunzip_nx_test gzip_nxfht_test gzip_nxdht_test
NXFLAGS = -DNXDBG  #-DNXDBG -DNXTIMER -DNX_MMAP

all:	$(TESTS)

gzip_vas.o:	../gzip_vas.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../gzip_vas.c

gzip_nxfht.o:	gzip_nxfht.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxfht.c

nx_dht.o:	../nx_dht.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../nx_dht.c

nx_dht_builtin.o:	../nx_dht_builtin.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../nx_dht_builtin.c

gzip_nxdht.o:	gzip_nxdht.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxdht.c -DSAVE_LZCOUNTS

gunzip_nx.o:	gunzip_nx.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gunzip_nx.c

gzip_nxfht_test: 	gzip_nxfht.o gzip_vas.o
	gcc $(CFLAGS) -o gzip_nxfht_test gzip_nxfht.o gzip_vas.o

gzip_nxdht_test: 	gzip_nxdht.o gzip_vas.o nx_dht.o nx_dht_builtin.o
	gcc $(CFLAGS) -lm -o gzip_nxdht_test gzip_nxdht.o gzip_vas.o nx_dht.o nx_dht_builtin.o

gunzip_nx_test: 	gunzip_nx.o gzip_vas.o
	gcc $(CFLAGS) -o gunzip_nx_test gunzip_nx.o gzip_vas.o

clean:
	rm -f $(TESTS) *.o *.c~ *.h~ Makefile~

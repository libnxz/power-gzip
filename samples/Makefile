CFLAGS = -O3
SRC =
INC = ../inc_nx
TESTS = gunzip_nx_test gzip_nxfht_test
NXFLAGS =  #-DNXDBG -DNXTIMER -DNX_MMAP

all:	$(TESTS)

gzip_vas.o:	../gzip_vas.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../gzip_vas.c

gzip_nxfht.o:	gzip_nxfht.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxfht.c

gunzip_nx.o:	gunzip_nx.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gunzip_nx.c

gzip_nxfht_test: 	gzip_nxfht.o gzip_vas.o
	gcc $(CFLAGS) -o gzip_nxfht_test gzip_nxfht.o gzip_vas.o

gunzip_nx_test: 	gunzip_nx.o gzip_vas.o
	gcc $(CFLAGS) -o gunzip_nx_test gunzip_nx.o gzip_vas.o

clean:
	rm -f $(TESTS) *.o *.c~ *.h~ Makefile~

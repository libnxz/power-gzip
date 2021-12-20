include ../config.mk

SRC =
INC = ../inc_nx
LDFLAGS = -L../lib
LDLIB = ../lib/libnxz.a -lpthread
TESTS = gunzip_nx_test gzip_nxfht_test gzip_nxdht_test compdecomp_th \
	bad_irq_check rand_pfault_check
NXFLAGS = #-DNXDBG  #-DNXDBG -DNX_MMAP

all:

bench: $(TESTS) zpipe gzm nx_gzip bench_initend

bench_initend: bench_initend.c
	$(CC) $(CFLAGS) -I$(INC) -o $@ $< -lz

gzip_nxfht.o:	gzip_nxfht.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxfht.c

#define SAVE_LZCOUNTS only when printing dhts in the cache
gzip_nxdht.o:	gzip_nxdht.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_nxdht.c #-DSAVE_LZCOUNTS

gunzip_nx.o:	gunzip_nx.c
	$(CC) $(CFLAGS) $(NXFLAGS) -I$(INC) -c gunzip_nx.c

gzip_nxfht_test: gzip_nxfht.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIB) -ldl

gzip_nxdht_test: gzip_nxdht.o
	$(CC) $(CFLAGS) -lm -o $@ $^ $(LDLIB) -ldl

gunzip_nx_test: gunzip_nx.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIB) -ldl

zpipe:	zpipe.c ../lib/libnxz.a
	$(CC) $(CFLAGS) -I../inc_nx -I../lib $(LDFLAGS) -L/usr/lib/ -o zpipe zpipe.c $(LDLIB) -ldl

zpipe_dict:  zpipe_dict.c
	$(CC) $(CFLAGS) -o zpipe_dict_zlib zpipe_dict.c -lz
	$(CC) $(CFLAGS) -I../inc_nx -I../lib $(LDFLAGS) -L/usr/lib/ -o zpipe_dict_nx zpipe_dict.c $(LDLIB) -ldl

crc_perf_test:  crc_perf_test.c
	$(CC) $(CFLAGS) -o crc_perf_test_zlib $(LDFLAGS) crc_perf_test.c -lz
	$(CC) $(CFLAGS) -o crc_perf_test_vmx $(LDFLAGS) crc_perf_test.c $(LDLIB) -ldl

makedata:  makedata.c
	$(CC) $(CFLAGS) -o makedata makedata.c

gzm:	gzm.c ../lib/libnxz.a
	$(CC) $(CFLAGS) -I../inc_nx -I../lib $(LDFLAGS) -L/usr/lib/ -o gzm gzm.c $(LDLIB) -ldl

compdecomp:	compdecomp.c ../lib/libnxz.a
	$(CC) $(CFLAGS) -I../inc_nx -I../lib $(LDFLAGS) -L/usr/lib/ -o compdecomp compdecomp.c $(LDLIB) -ldl

compdecomp_th: compdecomp_th.c  ../lib/libnxz.a
	$(CC) $(CFLAGS) -I../inc_nx -I../lib $(LDFLAGS) -L/usr/lib/ -o compdecomp_th compdecomp_th.c $(LDLIB) -ldl

bad_irq_check: bad_irq_check.c compdecomp_th.c ../lib/libnxz.a
	$(CC) $(CFLAGS) -Wl,-wrap,nx_touch_pages -Wl,-wrap,nx_touch_pages_dde \
	-I../inc_nx -I../lib $(LDFLAGS) -L/usr/lib/ -o bad_irq_check \
	bad_irq_check.c compdecomp_th.c $(LDLIB) -ldl

rand_pfault_check: rand_pfault_check.c compdecomp_th.c ../lib/libnxz.a
	$(CC) $(CFLAGS) -Wl,-wrap,nx_touch_pages -Wl,-wrap,nx_touch_pages_dde \
	-I../inc_nx -I../lib $(LDFLAGS) -L/usr/lib/ -o rand_pfault_check \
	rand_pfault_check.c compdecomp_th.c $(LDLIB) -ldl

nx_gzip: nx_gzip.c
	$(CC) $(CFLAGS) -I$(INC) $(LDFLAGS) -o $@ $^ $(LDLIB) -ldl

clean:
	rm -f $(TESTS) *.o *.c~ *.h~ Makefile~ zpipe compdecomp compdecomp_th makedata \
	zpipe_dict_nx zpipe_dict_zlib crc_perf_test_zlib crc_perf_test_vmx gzm nx_gzip \
	bench_initend

check:

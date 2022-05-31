AM_CPPFLAGS = -I$(top_srcdir)/inc_nx -I$(top_srcdir)/lib
LDADD= ../lib/libnxz.la $(PTHREAD_LIBS) -ldl

EXTRATESTS = gunzip_nx_test gzip_nxfht_test gzip_nxdht_test compdecomp_th \
	bad_irq_check rand_pfault_check
NXFLAGS = #-DNXDBG  #-DNXDBG -DNX_MMAP

EXTRA_PROGRAMS = bad_irq_check \
		 bench_initend \
		 compdecomp \
		 compdecomp_th \
		 crc_perf_test_vmx \
		 crc_perf_test_zlib \
		 gunzip_nx_test \
		 gzip_nxdht_test \
		 gzip_nxfht_test \
		 gzm \
		 makedata \
		 nx_gzip \
		 rand_pfault_check \
		 zpipe

# Does nothing by default.
all:

bench: $(EXTRATESTS) zpipe gzm nx_gzip bench_initend

bad_irq_check_SOURCES = bad_irq_check.c compdecomp_th.c
bad_irq_check_LDFLAGS = -Wl,-wrap,_nx_touch_pages -Wl,-wrap,_nx_touch_pages_dde

bench_initend_SOURCES = bench_initend.c

compdecomp_SOURCES = compdecomp.c

compdecomp_th_SOURCES = compdecomp_th.c

crc_perf_test_zlib_SOURCES = crc_perf_test.c

crc_perf_test_vmx_SOURCES = crc_perf_test.c

gunzip_nx_test_SOURCES = gunzip_nx.c samples_utils.c

gzip_nxfht_test_SOURCES = gzip_nxfht.c samples_utils.c

gzip_nxdht_test_SOURCES = gzip_nxdht.c samples_utils.c

gzm_SOURCES = gzm.c

makedata_SOURCES = makedata.c

nx_gzip_SOURCES = nx_gzip.c

rand_pfault_check_SOURCES = rand_pfault_check.c compdecomp_th.c samples_utils.c
rand_pfault_check_LDFLAGS = -Wl,-wrap,_nx_touch_pages \
                           -Wl,-wrap,_nx_touch_pages_dde

zpipe_SOURCES = zpipe.c

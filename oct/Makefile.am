TIMEOUT = 300s

TESTS =
check_FILES =
check_SCRIPTS = generate-test.sh generate-source.sh config.sh
# Move GNU Make conditionals away from Automake.
$(eval include $(srcdir)/tests.mk)

# db2 tests usually takes ~140m.
db2_TIMEOUT=280m

check_PROGRAMS = minideflate minigzipsh

minigzipsh_SOURCES = minigzip.c
minigzipsh_CFLAGS = -DUSE_MMAP=1
minigzipsh_LDFLAGS = -lz

all: check

.PHONY: download
download: $(uncomp_files)

minideflate.c:
	@$(WGET) \
	https://raw.githubusercontent.com/tuliom/zlib-ng/minideflate-fix/test/$@

minideflate$(EXEEXT): minideflate.c
	@sed -i 's/^#include.*zbuild\.h.*$$//g' $<
	$(CC) -O3 -Wall -DZLIB_COMPAT -D'PREFIX(x)=x' -D'PREFIX3(x)=z_##x' -o $@ $< -lz

minigzip.c:
	@$(WGET) \
	https://raw.githubusercontent.com/madler/zlib/master/test/$@

.PRECIOUS: %.uncompressed
$(srcdir)/empty.uncompressed:
	cd $(srcdir) && dd if=/dev/null bs=1k count=1 of=$@

$(srcdir)/random4k.uncompressed:
	cd $(srcdir) && dd if=/dev/urandom bs=4k count=1 of=$@

$(srcdir)/random13M.uncompressed:
	cd $(srcdir) && dd if=/dev/urandom bs=1M count=13 of=$@

$(srcdir)/sparse10M.uncompressed:
	cd $(srcdir) && dd if=/dev/urandom bs=1 count=0 seek=10M of=$@

$(srcdir)/sparse1000M.uncompressed:
	cd $(srcdir) && dd if=/dev/urandom bs=1 count=0 seek=1000M of=$@

$(srcdir)/zero4k.uncompressed:
	cd $(srcdir) && dd if=/dev/zero bs=4k count=1 of=$@

$(srcdir)/zero13M.uncompressed:
	cd $(srcdir) && dd if=/dev/zero bs=1M count=13 of=$@


%.uncompressed: %.source | download.sh config.sh
	cd $(srcdir) && CONFIG=${abs_builddir}/config.sh ./download.sh $*

.PRECIOUS: %.compress.deflate %.decompress.deflate %.compdecomp.deflate
%.compress.deflate.test: generate-test.sh config.sh minideflate $(check_FILES)
	CONFIG=${abs_builddir}/config.sh $(srcdir)/generate-test.sh $@

%.decompress.deflate.test: generate-test.sh config.sh minideflate $(check_FILES)
	CONFIG=${abs_builddir}/config.sh $(srcdir)/generate-test.sh $@

%.compdecomp.deflate.test: generate-test.sh config.sh minideflate $(check_FILES)
	CONFIG=${abs_builddir}/config.sh $(srcdir)/generate-test.sh $@

.PRECIOUS: %.compress.gzip %.decompress.gzip %.compdecomp.gzip
%.compress.gzip.test: generate-test.sh config.sh minigzipsh $(check_FILES)
	CONFIG=${abs_builddir}/config.sh $(srcdir)/generate-test.sh $@

%.decompress.gzip.test: generate-test.sh config.sh minigzipsh $(check_FILES)
	CONFIG=${abs_builddir}/config.sh $(srcdir)/generate-test.sh $@

%.compdecomp.gzip.test: generate-test.sh config.sh minigzipsh $(check_FILES)
	CONFIG=${abs_builddir}/config.sh $(srcdir)/generate-test.sh $@

# Pre-compute the checksum of all files in parallel.
.PRECIOUS: %.checksum
%.checksum: $(srcdir)/%.uncompressed
	@$(SHA256SUM) $? | $(AWK) '{ print $$1 }' > $@

check-am: all-am
	$(MAKE) $(AM_MAKEFLAGS) $(check_PROGRAMS) $(check_SCRIPTS) \
		$(check_FILES)
	$(MAKE) $(AM_MAKEFLAGS) check-TESTS


.PHONY: clean clean-all distclean
clean:
	@echo "Removing all tests..."
	@rm -f $(compress_tests) $(decompress_tests) $(compdecomp_tests)
	rm -f *.log
	rm -f *.checksum

distclean: clean

clean-all:
	rm $(uncomp_files)
	rm -f minideflate*

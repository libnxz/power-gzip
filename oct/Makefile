include config.mk

TIMEOUT = 300s

ifdef SOURCES
source_files = $(SOURCES)
else
# List of pre-created files.
source_files = at \
               calgary \
               initrd \
               linux \
               Packages \
               patch \
               silesia-dickens \
               silesia-mozilla \
               silesia-mr \
               silesia-nci \
               silesia-ooffice \
               silesia-osdb \
               silesia-reymont \
               silesia-samba \
               silesia-sao \
               silesia-webster \
               silesia-xml \
               silesia-x-ray \
               vmlinux

# Do not enable large files by default.
# They consume a lot of disk space and increase the execution time.
ifdef LARGEFILES
source_files += db2
endif
endif


# List of all files.  Including the auto-generated files.
files = $(source_files) \
        empty \
        random4k random13M \
        sparse10M sparse1000M \
        zero4k zero13M
uncomp_files = $(addsuffix .uncompressed,$(files))

test_types = deflate gzip
levels = 1 2 3 4 5 6 7 8 9
compress_tests = $(foreach t,$(test_types),\
                   $(foreach f,$(files),$(f).6.compress.$(t)))

decompress_tests = $(foreach t,$(test_types),\
                     $(foreach l,$(levels),\
                       $(foreach f,$(files),$(f).$(l).decompress.$(t))))

compdecomp_tests = $(foreach t,$(test_types),\
                     $(foreach f,$(files),$(f).6.compdecomp.$(t)))

# db2 tests usually takes ~140m.
db2_TIMEOUT=280m

.PHONY: all check
all: check

check: compress decompress compdecomp

.PHONY: compress decompress compdecomp
compress: $(addsuffix .test-result,$(compress_tests))
	@echo "Summary of $@ tests"; \
	 echo -e "$$(cat $? | grep -c PASS)\t PASS"; \
	 echo -e "$$(cat $? | grep -c FAIL)\t FAIL";

decompress: $(addsuffix .test-result,$(decompress_tests))
	@echo "Summary of $@ tests"; \
	 echo -e "$$(cat $? | grep -c PASS)\t PASS"; \
	 echo -e "$$(cat $? | grep -c FAIL)\t FAIL";

compdecomp: $(addsuffix .test-result,$(compdecomp_tests))
	@echo "Summary of $@ tests"; \
	 echo -e "$$(cat $? | grep -c PASS)\t PASS"; \
	 echo -e "$$(cat $? | grep -c FAIL)\t FAIL";

.PHONY: download
download: $(uncomp_files)

minideflate.c:
	@wget https://raw.githubusercontent.com/zlib-ng/zlib-ng/develop/test/$@

minideflate: minideflate.c
	@sed -i 's/^#include.*zbuild\.h.*$$//g' $<
	$(CC) -O3 -Wall -DZLIB_COMPAT -D'PREFIX(x)=x' -D'PREFIX3(x)=z_##x' -o $@ $< -lz

.PRECIOUS: %.uncompressed
empty.uncompressed:
	dd if=/dev/null bs=1k count=1 of=$@

random4k.uncompressed:
	dd if=/dev/urandom bs=4k count=1 of=$@

random13M.uncompressed:
	dd if=/dev/urandom bs=1M count=13 of=$@

sparse10M.uncompressed:
	dd if=/dev/urandom bs=1 count=0 seek=10M of=$@

sparse1000M.uncompressed:
	dd if=/dev/urandom bs=1 count=0 seek=1000M of=$@

zero4k.uncompressed:
	dd if=/dev/zero bs=4k count=1 of=$@

zero13M.uncompressed:
	dd if=/dev/zero bs=1M count=13 of=$@



%.uncompressed: %.source
	./download.sh $*

.PRECIOUS: %.compress.deflate %.decompress.deflate %.compdecomp.deflate
%.compress.deflate: generate-test.sh config.sh minideflate
	./generate-test.sh $@

%.decompress.deflate: generate-test.sh config.sh minideflate
	./generate-test.sh $@

%.compdecomp.deflate: generate-test.sh config.sh minideflate
	./generate-test.sh $@

.PRECIOUS: %.compress.gzip %.decompress.gzip %.compdecomp.gzip
%.compress.gzip: generate-test.sh config.sh
	./generate-test.sh $@

%.decompress.gzip: generate-test.sh config.sh
	./generate-test.sh $@

%.compdecomp.gzip: generate-test.sh config.sh
	./generate-test.sh $@

# Pre-compute the checksum of all files in parallel.
.PRECIOUS: %.checksum
%.checksum: %.uncompressed
	@$(SHA256SUM) $? | $(AWK) '{ print $1 }' > $@

.PHONY: clean clean-all
clean:
	@echo "Removing all tests..."
	@rm -f $(compress_tests) $(decompress_tests) $(compdecomp_tests)
	rm -f *.log
	rm -f *.test-result
	rm -f *.checksum

clean-all:
	rm $(uncomp_files)
	rm -f minideflate*

.SECONDEXPANSION:
%.test-result: % $$(shell echo -n % | cut -f1 -d.).checksum
	@echo $<
	@{ f=$($(shell echo -n $@ | cut -f1 -d.)_TIMEOUT); \
	   f=$${f:-$(TIMEOUT)}; \
	   if timeout -s TERM -k 5s $${f} \
	       ./$< &> $<.log; then echo PASS; else echo FAIL; fi }> $@

#include config.mk
include aminclude.am

# List of directories built by default.
SUBDIRS = lib test samples

DIST_SUBDIRS = doc inc_nx m4 oct

dist_doc_DATA = licenses/APACHE-2.0.txt licenses/gpl-2.0.txt

include_HEADERS = libnxz.h

if DX_COND_doc
doc_target = doc
endif DX_COND_doc

all: $(SUBDIRS) $(doc_target)

$(SUBDIRS):
	@if [ -d $@ ]; then				\
		$(MAKE) -C $@ || exit 1;	 	\
	fi

bench: $(SUBDIRS)
	$(MAKE) -C samples $@

check:  $(SUBDIRS)
	$(MAKE) -C test $@

valgrind-check:  $(SUBDIRS)
	$(MAKE) -C test check TEST_WRAPPER="${VALGRIND} --leak-check=full --suppressions=valgrind.supp --error-exitcode=1"

if DX_COND_doc
doc: doc/libnxz.doxy
	doxygen $<

MOSTLYCLEANFILES = $(DX_CLEANFILES)
endif DX_COND_doc

unsafe-check:  $(SUBDIRS)
	$(MAKE) -C test $@

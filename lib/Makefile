include ../config.mk

OBJS = nx_inflate.o nx_deflate.o nx_zlib.o nx_crc.o nx_dht.o nx_dhtgen.o \
       nx_dht_builtin.o nx_adler32.o gzip_vas.o nx_compress.o nx_uncompr.o \
       crc32_ppc.o crc32_ppc_asm.o nx_utils.o sw_zlib.o

SOVERSION = $(shell echo $(VERSION) | cut -d . -f 1)
STATICLIB = $(PACKAGENAME).a
SHAREDSONAMELIB = $(PACKAGENAME).so.$(SOVERSION)
SHAREDLIB = $(PACKAGENAME).so.$(VERSION)
LIBLINK = $(PACKAGENAME).so

CFLAGS += -I../inc_nx

all: $(OBJS) $(STATICLIB) $(SHAREDLIB)

$(STATICLIB): $(OBJS)
	rm -f $@
	$(AR) rcs -o $@ $(OBJS)

$(SHAREDLIB): $(OBJS)
	rm -f $@ $(LIBLINK) $(SHAREDSONAMELIB)
	$(CC) -shared  -Wl,-soname,$(SHAREDSONAMELIB),--version-script,Versions -o $@ $(OBJS) -ldl
	ln -s $@ $(LIBLINK)
	ln -s $@ $(SHAREDSONAMELIB)

clean:
	rm -f *.o *.gcda *.gcno *.so* *.a *~

install: $(STATICLIB) $(SHAREDLIB)
	install -d $(LIBDIR)
	install -m 755 $(SHAREDLIB) $(LIBDIR)
	{ cd $(LIBDIR); \
	  ln -s $(SHAREDLIB) $(SHAREDSONAMELIB); \
	  ln -s $(SHAREDLIB) $(LIBLINK); }
	install -m 644 $(STATICLIB) $(LIBDIR)

uninstall:
	rm $(LIBDIR)/$(SHAREDLIB)
	rm $(LIBDIR)/$(SHAREDSONAMELIB)
	rm $(LIBDIR)/$(STATICLIB)

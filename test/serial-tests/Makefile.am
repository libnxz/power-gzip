AM_COLOR_TESTS=always
AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/inc_nx -I$(top_srcdir)/test
LDADD = ../../lib/libnxz.la $(PTHREAD_LIBS) -ldl

# The tests listed here can cause other tests running in parallel to fail, so
# they need to be run serially
.NOTPARALLEL:

TESTS = test_exhaust_credits

check_PROGRAMS = exhaust_credits exhaust_credits_nx

exhaust_credits_SOURCES = exhaust_credits.c credit_utils.c ../test_utils.c
exhaust_credits_nx_SOURCES = exhaust_credits.c credit_utils.c ../test_utils.c
exhaust_credits_nx_CPPFLAGS = -DWITHPREFIX

test_exhaust_credits: exhaust_credits exhaust_credits_nx

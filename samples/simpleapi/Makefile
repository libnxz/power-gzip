CFLAGS = -O3
SRC =
INC = ../../inc_nx
TESTS = gzip_simple_test
#NXFLAGS = -DP9DBG

all:	$(TESTS)

gzip_vas.o:	../../gzip_vas.c 
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c ../../gzip_vas.c

gzip_simple_driver.o:	gzip_simple_driver.c
	gcc $(CFLAGS) $(NXFLAGS) -I$(INC) -c gzip_simple_driver.c

gzip_simple_test:	gzip_vas.o gzip_simple_driver.o
	gcc $(CFLAGS) -o $@ gzip_vas.o gzip_simple_driver.o -lpthread

clean:
	rm -f $(TESTS) *.o *.c~ *.h~ Makefile~

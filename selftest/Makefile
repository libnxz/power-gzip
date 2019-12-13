CC = gcc
CFLAGS = -O3
INC = ./inc
TESTS = gzfht_test gunz_test

all:	$(TESTS)

gzfht_test:	gzfht_test.c
	$(CC) $(CFLAGS)  -DNX_NO_CPU_PRI -I$(INC) -o gzfht_test gzfht_test.c gzip_vas.c

gunz_test:	gunz_test.c
	$(CC) $(CFLAGS)  -DNX_NO_CPU_PRI -I$(INC) -o gunz_test gunz_test.c gzip_vas.c

clean:
	rm -f $(TESTS) *.o *~

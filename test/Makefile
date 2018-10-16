#CC=gcc
CC=/opt/at11.0/bin/gcc
FLG= -std=gnu11
CFLAGS=-O3 $(FLG)

EXE=test_inflate \
    test_inflate_large

all: $(EXE)

test_inflate: test_inflate.c
	$(CC) $(CFLAGS) -L../ -o $@ $^ -lz -lnxz
test_inflate_large: test_inflate_large.c
	$(CC) $(CFLAGS) -L../ -o $@ $^ -lz -lnxz

clean:
	/bin/rm -f *.o $(EXE) *.log


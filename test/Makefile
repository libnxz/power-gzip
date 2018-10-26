CC = gcc
FLG = -std=gnu11
CFLAGS = -O3 $(FLG)

INC = -I../ -I../inc_nx
LIB = -L../ -L/usr/lib/

SRC_DEFLATE = ${wildcard test_deflate.c test_utils.c deflate/*.c}
SRC_INFLATE = ${wildcard test_inflate.c test_utils.c inflate/*.c}
OBJ_DEFLATE = ${patsubst %.c, %.o, $(SRC_DEFLATE)}
OBJ_INFLATE = ${patsubst %.c, %.o, $(SRC_INFLATE)}

SRC_TOOLS   = uncompr.c
OBJ_TOOLS   = ${patsubst %.c, %.o, $(SRC_TOOLS)}

EXE = test_inflate test_deflate uncompr

all: $(EXE)

test_inflate: $(OBJ_INFLATE)
	$(CC) $(CFLAGS) $(INC) $(LIB) -o $@ $^ -lz -lnxz
test_deflate: $(OBJ_DEFLATE)
	$(CC) $(CFLAGS) $(INC) $(LIB) -o $@ $^ -lz -lnxz
uncompr: $(OBJ_TOOLS)
	$(CC) $(CFLAGS) $(INC) $(LIB) -o $@ $^ -lz -lnxz

$(OBJ_DEFLATE): %.o : %.c
	$(CC) $(CFLAGS) $(INC) -c $^ -o $@
$(OBJ_INFLATE): %.o : %.c
	$(CC) $(CFLAGS) $(INC) -c $^ -o $@
$(OBJ_TOOLS): %.o : %.c
	$(CC) $(CFLAGS) $(INC) -c $^ -o $@

clean:
	/bin/rm -f *.o deflate/*.o inflate/*.o $(EXE) *.log


CC = gcc
FLG = -std=gnu11
CFLAGS = -O3 $(FLG)

INC = -I../ -I../inc_nx
LIB = -L../ -L/usr/lib/

SRC_DEFLATE = ${wildcard test_deflate.c test_utils.c deflate/*.c}
SRC_INFLATE = ${wildcard test_inflate.c test_utils.c inflate/*.c}
SRC_STRESS  = ${wildcard test_stress.c  test_utils.c}
obj_test_deflate = ${patsubst %.c, %.o, $(SRC_DEFLATE)}
obj_test_inflate = ${patsubst %.c, %.o, $(SRC_INFLATE)}
obj_test_stress  = ${patsubst %.c, %.o, $(SRC_STRESS)}

TEST_OBJS = $(sort $(obj_test_deflate) $(obj_test_inflate) $(obj_test_stress))
EXE = test_inflate test_deflate test_stress

all: $(EXE)

check: $(EXE)
	./run_test.sh

$(TEST_OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(INC) -c $^ -o $@

clean:
	/bin/rm -f *.o deflate/*.o inflate/*.o $(EXE) *.log

.SECONDEXPANSION:
test_%: $$(obj_$$@)
	$(CC) $(CFLAGS) $(INC) $(LIB) -o $@ $^  -lnxz -lpthread

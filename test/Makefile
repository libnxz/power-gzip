include ../config.mk

CFLAGS = $(FLG) $(SFLAGS)
INC = -I../lib -I../inc_nx
LIB = -L../lib -L/usr/lib/

SRC_DEFLATE = ${wildcard test_deflate.c test_utils.c deflate/*.c}
SRC_INFLATE = ${wildcard test_inflate.c test_utils.c inflate/*.c}
SRC_STRESS  = ${wildcard test_stress.c  test_utils.c}
obj_test_deflate = ${patsubst %.c, %.o, $(SRC_DEFLATE)}
obj_test_inflate = ${patsubst %.c, %.o, $(SRC_INFLATE)}
obj_test_stress  = ${patsubst %.c, %.o, $(SRC_STRESS)}
obj_test_crc32   = ${patsubst %.c, %.o, test_crc32.c}
obj_test_adler32 = ${patsubst %.c, %.o, test_adler32.c}
obj_test_multithread_stress = ${patsubst %.c, %.o, test_multithread_stress.c}

TEST_OBJS = $(sort $(obj_test_deflate) $(obj_test_inflate) $(obj_test_stress) \
    $(obj_test_crc32) $(obj_test_adler32) $(obj_test_multithread_stress))
EXE = test_inflate test_deflate test_stress test_crc32 test_adler32 test_multithread_stress
UNSAFE = test_stress

all: $(EXE)

check: $(EXE)
	./run_test.sh

unsafe-check: $(UNSAFE)
	./run_system_check.sh

$(TEST_OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(INC) -c $^ -o $@

clean:
	/bin/rm -f *.o deflate/*.o inflate/*.o $(EXE) $(UNSAFE) *.log

.SECONDEXPANSION:
test_%: $$(obj_$$@)
	$(CC) $(CFLAGS) $(INC) $(LIB) -o $@ $^  -lnxz -lpthread

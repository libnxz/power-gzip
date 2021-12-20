include ../config.mk

INC = -I../lib -I../inc_nx
LIB = -L../lib -L/usr/lib/

SRC_DEFLATE = ${wildcard test_deflate.c test_utils.c deflate/*.c}
SRC_INFLATE = ${wildcard test_inflate.c test_utils.c inflate/*.c}
SRC_STRESS  = ${wildcard test_stress.c  test_utils.c}
SRC_INFLATESYNCPOINT = ${wildcard test_inflatesyncpoint.c test_utils.c}
SRC_ZEROINPUT = ${wildcard test_zeroinput.c test_utils.c}
SRC_BUF_ERROR = ${wildcard test_buf_error.c test_utils.c}
obj_test_deflate = ${patsubst %.c, %.o, $(SRC_DEFLATE)}
obj_test_inflate = ${patsubst %.c, %.o, $(SRC_INFLATE)}
obj_test_stress  = ${patsubst %.c, %.o, $(SRC_STRESS)}
obj_test_crc32   = ${patsubst %.c, %.o, test_crc32.c}
obj_test_adler32 = ${patsubst %.c, %.o, test_adler32.c}
obj_test_multithread_stress = ${patsubst %.c, %.o, test_multithread_stress.c}
obj_test_pid_reuse = ${patsubst %.c, %.o, test_pid_reuse.c}
obj_test_inflatesyncpoint = ${patsubst %.c, %.o, $(SRC_INFLATESYNCPOINT)}
obj_test_zeroinput = ${patsubst %.c, %.o, $(SRC_ZEROINPUT)}
obj_test_buf_error = ${patsubst %.c, %.o, $(SRC_BUF_ERROR)}

TEST_OBJS = $(sort $(obj_test_deflate) $(obj_test_inflate) $(obj_test_stress) \
    $(obj_test_crc32) $(obj_test_adler32) $(obj_test_multithread_stress) \
    $(obj_test_inflatesyncpoint) $(obj_test_zeroinput) $(obj_test_buf_error))
EXE = test_inflate test_deflate test_stress test_crc32 test_adler32 \
    test_multithread_stress test_pid_reuse test_inflatesyncpoint \
	test_zeroinput test_buf_error
UNSAFE = test_stress

ABIDW_FLAGS = --no-corpus-path --drop-undefined-syms --drop-private-types \
    --no-comp-dir-path --no-show-locs

all:
install:

check: $(EXE)
	./run_test.sh

unsafe-check: $(UNSAFE)
	./run_system_check.sh

$(TEST_OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(INC) -c $^ -o $@

regen-abi:
	@rm -f libnxz.abi
	abidw $(ABIDW_FLAGS) ../lib/libnxz.so > libnxz.abi
	@echo "To regenerate libz.abi, build zlib or find system's libz.so and do:"
	@echo "    abidw $(ABIDW_FLAGS) /path/to/libz.so > libz.abi"

clean:
	/bin/rm -f *.o deflate/*.o inflate/*.o $(EXE) $(UNSAFE) *.log

.SECONDEXPANSION:
test_%: $$(obj_$$@)
	$(CC) $(CFLAGS) $(INC) $(LIB) -o $@ $^  -lnxz -lpthread -ldl

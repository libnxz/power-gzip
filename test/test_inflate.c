#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <endian.h>
#include <zlib.h>

#include "test.h"
#include "test_deflate.h"

#define check(s) do { if(s) exit(1); } while(0)

int main()
{
	check ( run_case2() );
	check ( run_case3() );
	check ( run_case4() );
	check ( run_case5() );
	check ( run_case6() );
	check ( run_case7() );
	check ( run_case8() );
	check ( run_case9() );
	check ( run_case9_1() );
}


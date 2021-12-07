# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# The Automake init of 1.10 requires at least autoconf 2.62
AC_PREREQ([2.69])

#Autoconf versioning is A:C:B
AC_INIT([libnxz],[0.62],[https://github.com/libnxz/power-gzip])

AC_CONFIG_SRCDIR([lib/nx_zlib.c])
AC_CONFIG_FILES([config.mk oct/config.mk])
AC_CONFIG_MACRO_DIR([m4])
# TODO: Leave this commented out while Automake is not being used.
#AC_CONFIG_HEADERS([config.h])
#AM_INIT_AUTOMAKE([1.10 no-define foreign subdir-objects])
#AM_MAINTAINER_MODE([disable])

# Defines host_cpu, host_vendor, and host_os variables.
AC_CANONICAL_HOST

: ${CFLAGS="-O3 -Wall -Werror -g"}

# Checks for programs.
AC_CHECK_TARGET_TOOL([AR], [ar], [:])
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_GREP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_SED
AX_WITH_PROG(BASH, bash)
AX_WITH_PROG(GZIP, gzip)
AX_WITH_PROG(SHA256SUM, sha256sum)
AX_WITH_PROG(WGET, wget)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([sys/file.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([sys/param.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([syslog.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([zlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([ftruncate])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([munmap])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([strtoull])
# Pthread check (on m4/acx_pthread.m4)
ACX_PTHREAD

AX_CHECK_COMPILE_FLAG([-mcpu=power9],
		      [CFLAGS="$CFLAGS -mcpu=power9"])

# Doxygen support
# TODO: Leave this commented out while Automake is not being used.
#DX_HTML_FEATURE(ON)
#DX_MAN_FEATURE(ON)
#DX_PDF_FEATURE(OFF)
#DX_INIT_DOXYGEN($PACKAGE_NAME, [doc/libnxz.doxy])

AC_OUTPUT
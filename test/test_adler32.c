/* adler_test.c -- unit test for adler32 in the zlib compression library
 * Copyright (C) 1995-2006, 2010, 2011, 2016, 2019 Rogerio Alves
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zlib.h"
#include <stdio.h>

#ifdef STDC
#  include <string.h>
#  include <stdlib.h>
#endif

void test_adler32  OF((uLong adler, Byte* buf, z_size_t len, uLong chk, int line));
int main         OF((void));

typedef struct {
    int line;
    uLong adler;
    Byte* buf;
    int len;
    uLong expect;
} adler32_test;

void test_adler32(adler, buf, len, chk, line)
    uLong adler;
    Byte *buf;
    z_size_t len;
    uLong chk;
    int line;
{
    uLong res = adler32(adler, buf, len);
    if (res != chk) {
        fprintf(stderr, "FAIL [%d]: adler32 returned 0x%08X expected 0x%08X\n",
                line, (unsigned int)res, (unsigned int)chk);
        exit(1);
    }
}

static const adler32_test tests[] = {
   {__LINE__,0x1, 0x0, 0, 0x1},
   {__LINE__,0x1, "", 1, 0x10001},
   {__LINE__,0x1, "a", 1, 0x620062},
   {__LINE__,0x1, "abacus", 6, 0x8400270},
   {__LINE__,0x1, "backlog", 7, 0xb1f02d4},
   {__LINE__,0x1, "campfire", 8, 0xea10348},
   {__LINE__,0x1, "delta", 5, 0x61a020b},
   {__LINE__,0x1, "executable", 10, 0x16fa0423},
   {__LINE__,0x1, "file", 4, 0x41401a1},
   {__LINE__,0x1, "greatest", 8, 0xefa0360},
   {__LINE__,0x1, "inverter", 8, 0xf6f0370},
   {__LINE__,0x1, "jigsaw", 6, 0x8bd0286},
   {__LINE__,0x1, "karate", 6, 0x8a50279},
   {__LINE__,0x1, "landscape", 9, 0x126a03ac},
   {__LINE__,0x1, "machine", 7, 0xb5302d6},
   {__LINE__,0x1, "nanometer", 9, 0x12d803ca},
   {__LINE__,0x1, "oblivion", 8, 0xf220363},
   {__LINE__,0x1, "panama", 6, 0x8a1026f},
   {__LINE__,0x1, "quest", 5, 0x6970233},
   {__LINE__,0x1, "resource", 8, 0xf8d0369},
   {__LINE__,0x1, "secret", 6, 0x8d10287},
   {__LINE__,0x1, "ultimate", 8, 0xf8d0366},
   {__LINE__,0x1, "vector", 6, 0x8fb0294},
   {__LINE__,0x1, "walrus", 6, 0x918029f},
   {__LINE__,0x1, "xeno", 4, 0x45e01bb},
   {__LINE__,0x1, "yelling", 7, 0xbfe02f5},
   {__LINE__,0x1, "zero", 4, 0x46e01c1},
   {__LINE__,0x1, "4BJD7PocN1VqX0jXVpWB", 20, 0x3eef064d},
   {__LINE__,0x1, "F1rPWI7XvDs6nAIRx41l", 20, 0x425d065f},
   {__LINE__,0x1, "ldhKlsVkPFOveXgkGtC2", 20, 0x4f1a073e},
   {__LINE__,0x1, "5KKnGOOrs8BvJ35iKTOS", 20, 0x42290650},
   {__LINE__,0x1, "0l1tw7GOcem06Ddu7yn4", 20, 0x43fd0690},
   {__LINE__,0x1, "MCr47CjPIn9R1IvE1Tm5", 20, 0x3f770609},
   {__LINE__,0x1, "UcixbzPKTIv0SvILHVdO", 20, 0x4c7c0703},
   {__LINE__,0x1, "dGnAyAhRQDsWw0ESou24", 20, 0x48ac06b7},
   {__LINE__,0x1, "di0nvmY9UYMYDh0r45XT", 20, 0x489a0698},
   {__LINE__,0x1, "2XKDwHfAhFsV0RhbqtvH", 20, 0x44a906e6},
   {__LINE__,0x1, "ZhrANFIiIvRnqClIVyeD", 20, 0x4a29071c},
   {__LINE__,0x1, "v7Q9ehzioTOVeDIZioT1", 20, 0x4a7706f9},
   {__LINE__,0x1, "Yod5hEeKcYqyhfXbhxj2", 20, 0x4ce60769},
   {__LINE__,0x1, "GehSWY2ay4uUKhehXYb0", 20, 0x48ae06e5},
   {__LINE__,0x1, "kwytJmq6UqpflV8Y8GoE", 20, 0x51d60750},
   {__LINE__,0x1, "70684206568419061514", 20, 0x2b100414},
   {__LINE__,0x1, "42015093765128581010", 20, 0x2a550405},
   {__LINE__,0x1, "88214814356148806939", 20, 0x2b450423},
   {__LINE__,0x1, "43472694284527343838", 20, 0x2b460421},
   {__LINE__,0x1, "49769333513942933689", 20, 0x2bc1042b},
   {__LINE__,0x1, "54979784887993251199", 20, 0x2ccd043d},
   {__LINE__,0x1, "58360544869206793220", 20, 0x2b68041a},
   {__LINE__,0x1, "27347953487840714234", 20, 0x2b84041d},
   {__LINE__,0x1, "07650690295365319082", 20, 0x2afa0417},
   {__LINE__,0x1, "42655507906821911703", 20, 0x2aff0412},
   {__LINE__,0x1, "29977409200786225655", 20, 0x2b8d0420},
   {__LINE__,0x1, "85181542907229116674", 20, 0x2b140419},
   {__LINE__,0x1, "87963594337989416799", 20, 0x2c8e043f},
   {__LINE__,0x1, "21395988329504168551", 20, 0x2b68041f},
   {__LINE__,0x1, "51991013580943379423", 20, 0x2af10417},
   {__LINE__,0x1, "*]+@!);({_$;}[_},?{?;(_?,=-][@", 30, 0x7c9d0841},
   {__LINE__,0x1, "_@:_).&(#.[:[{[:)$++-($_;@[)}+", 30, 0x71060751},
   {__LINE__,0x1, "&[!,[$_==}+.]@!;*(+},[;:)$;)-@", 30, 0x7095070a},
   {__LINE__,0x1, "]{.[.+?+[[=;[?}_#&;[=)__$$:+=_", 30, 0x82530815},
   {__LINE__,0x1, "-%.)=/[@].:.(:,()$;=%@-$?]{%+%", 30, 0x61250661},
   {__LINE__,0x1, "+]#$(@&.=:,*];/.!]%/{:){:@(;)$", 30, 0x642006a3},
   {__LINE__,0x1, ")-._.:?[&:.=+}(*$/=!.${;(=$@!}", 30, 0x674206cb},
   {__LINE__,0x1, ":(_*&%/[[}+,?#$&*+#[([*-/#;%(]", 30, 0x67670680},
   {__LINE__,0x1, "{[#-;:$/{)(+[}#]/{&!%(@)%:@-$:", 30, 0x7547070f},
   {__LINE__,0x1, "_{$*,}(&,@.)):=!/%(&(,,-?$}}}!", 30, 0x69ea06ee},
   {__LINE__,0x1, "e$98KNzqaV)Y:2X?]77].{gKRD4G5{mHZk,Z)SpU%L3FSgv!Wb8MLAFdi{+fp)c,@8m6v)yXg@]HBDFk?.4&}g5_udE*JHCiH=aL", 100, 0x1b01e92},
   {__LINE__,0x1, "r*Fd}ef+5RJQ;+W=4jTR9)R*p!B;]Ed7tkrLi;88U7g@3v!5pk2X6D)vt,.@N8c]@yyEcKi[vwUu@.Ppm@C6%Mv*3Nw}Y,58_aH)", 100, 0xfbdb1e96},
   {__LINE__,0x1, "h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&", 100, 0x47a61ec8},
   {__LINE__,0x7a30360d, 0x0, 0, 0x1},
   {__LINE__,0x6fd767ee, "", 1, 0xd7c567ee},
   {__LINE__,0xefeb7589, "a", 1, 0x65e475ea},
   {__LINE__,0x61cf7e6b, "abacus", 6, 0x60b880da},
   {__LINE__,0xdc712e2, "backlog", 7, 0x9d0d15b5},
   {__LINE__,0xad23c7fd, "campfire", 8, 0xfbfecb44},
   {__LINE__,0x85cb2317, "delta", 5, 0x3b622521},
   {__LINE__,0x9eed31b0, "executable", 10, 0xa6db35d2},
   {__LINE__,0xb94f34ca, "file", 4, 0x9096366a},
   {__LINE__,0xab058a2, "greatest", 8, 0xded05c01},
   {__LINE__,0x5bff2b7a, "inverter", 8, 0xc7452ee9},
   {__LINE__,0x605c9a5f, "jigsaw", 6, 0x7899ce4},
   {__LINE__,0x51bdeea5, "karate", 6, 0xf285f11d},
   {__LINE__,0x85c21c79, "landscape", 9, 0x98732024},
   {__LINE__,0x97216f56, "machine", 7, 0xadf4722b},
   {__LINE__,0x18444af2, "nanometer", 9, 0xcdb34ebb},
   {__LINE__,0xbe6ce359, "oblivion", 8, 0xe8b7e6bb},
   {__LINE__,0x843071f1, "panama", 6, 0x389e745f},
   {__LINE__,0xf2480c60, "quest", 5, 0x36c90e92},
   {__LINE__,0x2d2feb3d, "resource", 8, 0x9705eea5},
   {__LINE__,0x7490310a, "secret", 6, 0xa3a63390},
   {__LINE__,0x97d247d4, "ultimate", 8, 0xe6154b39},
   {__LINE__,0x93cf7599, "vector", 6, 0x5e87782c},
   {__LINE__,0x73c84278, "walrus", 6, 0xbc84516},
   {__LINE__,0x228a87d1, "xeno", 4, 0x4646898b},
   {__LINE__,0xa7a048d0, "yelling", 7, 0xb1654bc4},
   {__LINE__,0x1f0ded40, "zero", 4, 0xd8a4ef00},
   {__LINE__,0xa804a62f, "4BJD7PocN1VqX0jXVpWB", 20, 0xe34eac7b},
   {__LINE__,0x508fae6a, "F1rPWI7XvDs6nAIRx41l", 20, 0x33f2b4c8},
   {__LINE__,0xe5adaf4f, "ldhKlsVkPFOveXgkGtC2", 20, 0xe7b1b68c},
   {__LINE__,0x67136a40, "5KKnGOOrs8BvJ35iKTOS", 20, 0xf6a0708f},
   {__LINE__,0xb00c4a10, "0l1tw7GOcem06Ddu7yn4", 20, 0xbd8f509f},
   {__LINE__,0x2e0c84b5, "MCr47CjPIn9R1IvE1Tm5", 20, 0xcc298abd},
   {__LINE__,0x81238d44, "UcixbzPKTIv0SvILHVdO", 20, 0xd7809446},
   {__LINE__,0xf853aa92, "dGnAyAhRQDsWw0ESou24", 20, 0x9525b148},
   {__LINE__,0x5a692325, "di0nvmY9UYMYDh0r45XT", 20, 0x620029bc},
   {__LINE__,0x3275b9f, "2XKDwHfAhFsV0RhbqtvH", 20, 0x70916284},
   {__LINE__,0x38371feb, "ZhrANFIiIvRnqClIVyeD", 20, 0xd52706},
   {__LINE__,0xafc8bf62, "v7Q9ehzioTOVeDIZioT1", 20, 0xeeb4c65a},
   {__LINE__,0x9b07db73, "Yod5hEeKcYqyhfXbhxj2", 20, 0xde3e2db},
   {__LINE__,0xe75b214, "GehSWY2ay4uUKhehXYb0", 20, 0x4171b8f8},
   {__LINE__,0x72d0fe6f, "kwytJmq6UqpflV8Y8GoE", 20, 0xa66a05cd},
   {__LINE__,0xf857a4b1, "70684206568419061514", 20, 0x1f9a8c4},
   {__LINE__,0x54b8e14, "42015093765128581010", 20, 0x49c19218},
   {__LINE__,0xd6aa5616, "88214814356148806939", 20, 0xbbfc5a38},
   {__LINE__,0x11e63098, "43472694284527343838", 20, 0x93434b8},
   {__LINE__,0xbe92385, "49769333513942933689", 20, 0xfe1827af},
   {__LINE__,0x49511de0, "54979784887993251199", 20, 0xcba8221c},
   {__LINE__,0x3db13bc1, "58360544869206793220", 20, 0x14643fda},
   {__LINE__,0xbb899bea, "27347953487840714234", 20, 0x1604a006},
   {__LINE__,0xf6cd9436, "07650690295365319082", 20, 0xb69f984c},
   {__LINE__,0x9109e6c3, "42655507906821911703", 20, 0xc43eead4},
   {__LINE__,0x75770fc, "29977409200786225655", 20, 0x707751b},
   {__LINE__,0x69b1d19b, "85181542907229116674", 20, 0xf5bdd5b3},
   {__LINE__,0xc6132975, "87963594337989416799", 20, 0x2fed2db3},
   {__LINE__,0xd58cb00c, "21395988329504168551", 20, 0xc2a2b42a},
   {__LINE__,0xb63b8caa, "51991013580943379423", 20, 0xdf0590c0},
   {__LINE__,0x8a45a2b8, "*]+@!);({_$;}[_},?{?;(_?,=-][@", 30, 0x1980aaf8},
   {__LINE__,0xcbe95b78, "_@:_).&(#.[:[{[:)$++-($_;@[)}+", 30, 0xf58662c8},
   {__LINE__,0x4ef8a54b, "&[!,[$_==}+.]@!;*(+},[;:)$;)-@", 30, 0x1f65ac54},
   {__LINE__,0x76ad267a, "]{.[.+?+[[=;[?}_#&;[=)__$$:+=_", 30, 0x7b792e8e},
   {__LINE__,0x569e613c, "-%.)=/[@].:.(:,()$;=%@-$?]{%+%", 30, 0x1d61679c},
   {__LINE__,0x36aa61da, "+]#$(@&.=:,*];/.!]%/{:){:@(;)$", 30, 0x12ec687c},
   {__LINE__,0xf67222df, ")-._.:?[&:.=+}(*$/=!.${;(=$@!}", 30, 0x740329a9},
   {__LINE__,0x74b34fd3, ":(_*&%/[[}+,?#$&*+#[([*-/#;%(]", 30, 0x374c5652},
   {__LINE__,0x351fd770, "{[#-;:$/{)(+[}#]/{&!%(@)%:@-$:", 30, 0xeadfde7e},
   {__LINE__,0xc45aef77, "_{$*,}(&,@.)):=!/%(&(,,-?$}}}!", 30, 0x3fcbf664},
   {__LINE__,0xd034ea71, "e$98KNzqaV)Y:2X?]77].{gKRD4G5{mHZk,Z)SpU%L3FSgv!Wb8MLAFdi{+fp)c,@8m6v)yXg@]HBDFk?.4&}g5_udE*JHCiH=aL", 100, 0x6b080911},
   {__LINE__,0xdeadc0de, "r*Fd}ef+5RJQ;+W=4jTR9)R*p!B;]Ed7tkrLi;88U7g@3v!5pk2X6D)vt,.@N8c]@yyEcKi[vwUu@.Ppm@C6%Mv*3Nw}Y,58_aH)", 100, 0x355fdf73},
   {__LINE__,0xba5eba11, "h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&", 100, 0xb48bd8d8}
};

static const int test_size = sizeof(tests) / sizeof(tests[0]);

int main(void)
{
    int i;
    for (i = 0; i < test_size; i++) {
        test_adler32(tests[i].adler, tests[i].buf, tests[i].len,
                   tests[i].expect, tests[i].line);
    }
    return 0;
}

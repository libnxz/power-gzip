#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* change this to the output file size you desire */
#ifndef LOGFILESZ
#define LOGFILESZ 20
#endif
#ifndef OUTFILESZ
#define OUTFILESZ (1<<LOGFILESZ)
#endif

int main(int argc, char **argv)
{
	size_t bufsz = OUTFILESZ;
	size_t readsz, writesz;
	char *buf;
	size_t idx;
	int seed;

	if (argc == 3 && strcmp(argv[1], "-s") == 0) {
		seed = atoi(argv[2]);
	}	
	if ((argc == 1) || (argc == 2 && strcmp(argv[1], "-h") == 0)) {
		fprintf(stderr,"randomly produces a data file using a seed number and any input file\n");
		fprintf(stderr,"usage: %s -s rngseed < seedfile > outputfile\n", argv[0]);
		return -1;
	}
	
	assert(NULL != (buf = malloc(bufsz)));

	/* read up to half the buffer size */
	readsz = fread(buf, 1, bufsz/2, stdin);
	fprintf(stderr, "read bytes %ld\n", readsz);

	/* next free location */
	idx = readsz;
	
	srand48(seed);

	size_t len_max = lrand48() % 240;
	size_t dist_max = lrand48() % (1<<18);
	  
	while(idx < bufsz) {

		/* pick random point in the buffer and copy */
		size_t dist = lrand48() % ( (idx > dist_max) ? dist_max: idx );
		size_t len = (lrand48() % len_max) + 16;

		if (dist > idx) 
			dist = idx; /* out of bounds */

		/* copy */
		while (len-- > 0 && idx < bufsz) {
			buf[idx] = buf[idx-dist];
			++idx;
		}
	}

	writesz = fwrite(buf, 1, idx, stdout);
		
	return 0;
}


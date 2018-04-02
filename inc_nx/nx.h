
#define	NX_FUNC_COMP_842	1
#define NX_FUNC_COMP_GZIP	2

typedef int bool;

struct nx842_func_args {
	bool use_crc;
	bool decompress;		/* true: decompress; false compress */
	bool move_data;
	int timeout;			/* seconds */
};

typedef struct {
	int len;
	char *buf;
} nxbuf_t;

/* @function should be EFT (aka 842), GZIP etc */
extern void *nx_function_begin(int function, int pri);

extern int nx_function(void *handle, nxbuf_t *in, nxbuf_t *out, void *arg);

extern int nx_function_end(void *handle);


#define DATA_MAX_LEN (128*1024*1024) // 128M

extern char ran_data[DATA_MAX_LEN];
extern void generate_random_data(int len);
extern char* generate_allocated_random_data(unsigned int len);
extern void generate_all_data(int len, char digit);
extern int compare_data(char* src, char* dest, int len);

int _test_nx_deflate(Byte* src, unsigned int src_len, Byte* compr,
		     unsigned int* compr_len, int step);
int _test_deflate(Byte* src, unsigned int src_len, Byte* compr,
		  unsigned int* compr_len, int step);
int _test_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr,
		  unsigned int uncomprLen, Byte* src, unsigned int src_len,
		  int step);
int _test_nx_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr,
		     unsigned int uncomprLen, Byte* src, unsigned int src_len,
		     int step, int flush);

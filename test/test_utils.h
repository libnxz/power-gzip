
#define DATA_MAX_LEN (128*1024*1024) // 128M

extern char ran_data[DATA_MAX_LEN];
extern int generate_random_data(int len);
extern char* generate_allocated_random_data(unsigned int len);
extern int generate_all_data(int len, char digit);
extern int compare_data(char* src, char* dest, int len);


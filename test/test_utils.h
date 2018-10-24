
#define DATA_MAX_LEN (128*1024*1024) // 128M

extern char ran_data[DATA_MAX_LEN];
extern int generate_random_data(int len);
extern int compare_data(char* src, char* dest, int len);


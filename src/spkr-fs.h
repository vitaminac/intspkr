#include <linux/fs.h>

extern struct file_operations intspkr_fops;

void init_fs(void);
void destroy_fs(void);
#include "spkr-fs.h"
#include <linux/uaccess.h>

struct mutex mutexForWriteSession;
static int intspkr_open(struct inode *inode, struct file *file)
{
    if (file->f_mode & FMODE_WRITE)
    {
        if (mutex_trylock(&mutexForWriteSession) == 0)
        {
            return -EBUSY;
        }
    }
    //(*filp).private_data = kmalloc(1, GFP_KERNEL);
    //get_random_bytes((*filp).private_data, 1);
    printk(KERN_INFO "intspkr opened!\n");
    return 0;
}

static ssize_t intspkr_read(struct file *filp, char __user *buf, size_t size, loff_t *f_pos)
{
    printk(KERN_INFO "intspkr read!\n");
    return 0;
}

static ssize_t intspkr_write(struct file *filp, const char __user *buf, size_t size, loff_t *f_pos)
{
    /*char soundData[4];
    ssize_t count = 0;
    for (; (size - count) > 4; count += 4)
    {
        if (copy_from_user(soundData, buf + count, 4))
            return -EFAULT;
        //producesSound((((uint16_t)soundData[0]) << 8) | (((uint16_t)soundData[1])), (((uint16_t)soundData[2]) << 8) | (((uint16_t)soundData[3])));
    }*/
    printk(KERN_INFO "intspkr wrote!\n");
    return size;
}

static int intspkr_release(struct inode *inode, struct file *file)
{
    if (file->f_mode & FMODE_WRITE)
    {
        mutex_unlock(&mutexForWriteSession);
    }
    //kfree((*filp).private_data);
    printk(KERN_INFO "intspkr released!\n");
    return 0;
}

struct file_operations intspkr_fops = {
    .owner = THIS_MODULE,
    .open = intspkr_open,
    .release = intspkr_release,
    .write = intspkr_write,
    .read = intspkr_read,
};

void init_fs(void)
{
    mutex_init(&mutexForWriteSession);
}

void destroy_fs(void)
{
    mutex_destroy(&mutexForWriteSession);
}
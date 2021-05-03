#include <linux/uaccess.h>
#include <linux/slab.h>
#include "spkr-fs.h"
#include "spkr-fifo.h"

static ssize_t intspkr_read(struct file *file, char __user *buf, size_t size, loff_t *f_pos)
{
    printk(KERN_INFO "intspkr read!\n");
    return 0;
}

typedef struct
{
    char soundData[4];
    int copied;
} PrivateData;

static struct mutex mutexForWriteSession;

static int intspkr_open(struct inode *inode, struct file *file)
{
    if (file->f_mode & FMODE_WRITE)
    {
        if (mutex_trylock(&mutexForWriteSession) == 0)
        {
            return -EBUSY;
        }
    }
    file->private_data = kmalloc(sizeof(PrivateData), GFP_KERNEL);
    ((PrivateData *)(file->private_data))->copied = 0;
    printk(KERN_INFO "intspkr opened!\n");
    return 0;
}

static inline void playSound(char *soundData)
{
    putSound((((uint16_t)soundData[0]) << 8) | ((uint16_t)soundData[1]), (((uint16_t)soundData[2]) << 8) | ((uint16_t)soundData[3]));
}
static inline size_t copySoundDataFromUserSpace(PrivateData *privateData, const char __user *buf, const size_t remaining)
{
    size_t notCopied;
    size_t needToCopy = 4 - privateData->copied;
    needToCopy = needToCopy > remaining ? remaining : needToCopy;
    printk(KERN_INFO "Try to copy %d bytes from user space with %d offset", needToCopy, privateData->copied);
    notCopied = copy_from_user(privateData->soundData + privateData->copied, buf, needToCopy);
    printk(KERN_INFO "The result of copy_from_user is %d.\n", notCopied);
    if (notCopied > 0)
    {
        printk(KERN_INFO "intspkr failed to copy %d bytes from user space!\n", notCopied);
        return needToCopy - notCopied;
    }
    return needToCopy;
}
static ssize_t intspkr_write(struct file *file, const char __user *buf, size_t size, loff_t *f_pos)
{
    ssize_t count = 0;
    size_t copied;
    PrivateData *privateData = (PrivateData *)(file->private_data);
    printk(KERN_INFO "intspkr try to write %d bytes!\n", size);

    while (size > count)
    {
        copied = copySoundDataFromUserSpace(privateData, buf + count, size - count);
        count += copied;
        printk(KERN_INFO "intspkr has written %d bytes out of %d bytes!\n", count, size);
        privateData->copied += copied;
        if (privateData->copied == 4)
        {
            playSound(privateData->soundData);
            privateData->copied -= 4;
        }
    }
    printk(KERN_INFO "intspkr wrote %d bytes!\n", count);
    return count;
}

static int intspkr_release(struct inode *inode, struct file *file)
{
    if (file->f_mode & FMODE_WRITE)
    {
        mutex_unlock(&mutexForWriteSession);
    }
    kfree(file->private_data);
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
    init_fifo();
}

void destroy_fs(void)
{
    destroy_fifo();
    mutex_destroy(&mutexForWriteSession);
}
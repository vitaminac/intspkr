#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include "spkr-io.h"

MODULE_LICENSE("Dual BSD/GPL");

/**
 * recibiremos como parámetro el valor del minor,
 * que representa el minor del identificador de dispositivo que queremos reservar
 * usando 0 como valor por defecto.
 */
static int minor = 0;
module_param(minor, int, S_IRUGO);

static dev_t devID;

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

/**
 * la estructura de datos interna que representa un dispositivo de caracteres y, 
 * dentro de esta estructura, especificar la parte más importante: 
 * el conjunto de funciones de acceso (apertura, cierre, lectura, escritura...) que proporciona el dispositivo.
 * dentro del tipo struct cdev hay un campo denominado dev de tipo dev_t 
 * que almacena el identificador de ese dispositivo
 */
static struct cdev dev;
static struct file_operations intspkr_fops = {
    .owner = THIS_MODULE,
    .open = intspkr_open,
    .release = intspkr_release,
    .write = intspkr_write,
    .read = intspkr_read,
};

static struct class *dev_class;

#define FREQUENCY_OF_BEEP 440
#define DEVICE_NAME "intspkr"
#define SYSFS_CLASS_NAME_FOR_INTSPKR DEVICE_NAME
#define SYSFS_DEVICE_NAME_FOR_INTSPKR DEVICE_NAME
static int __init intspkr_init(void)
{
    // reservar la pareja major id y minor id y automaticamente anade una entra en /proc/devices
    if (alloc_chrdev_region(&devID, minor, 1, DEVICE_NAME) < 0)
    {
        // Si esta función, o cualquiera de las usadas en esta fase dentro de la rutina de inicio del módulo, devuelve un error, esa función de inicio debe terminar retornando un -1.
        return -1;
    }

    // crear un dispositivo de caracter asociado con el identificador reservado previamente
    cdev_init(&dev, &intspkr_fops);
    cdev_add(&dev, devID, 1);

    // habilita el uso desde applicaciones
    // dar alta la clase de sysfs /sys/class/intspkr
    if ((dev_class = class_create(THIS_MODULE, SYSFS_CLASS_NAME_FOR_INTSPKR)) == NULL)
    {
        return -1;
    }
    // dar alta el dispositivo /sys/class/intspkr/intspkr asociado a esta clase y automaticamente darse alta el archivo /dev/intspkr
    device_create(dev_class, NULL, devID, NULL, SYSFS_DEVICE_NAME_FOR_INTSPKR);

    mutex_init(&mutexForWriteSession);
    printk(KERN_INFO "Initialized intspkr!\n");

    spkr_init();
    spkr_set_frequency(FREQUENCY_OF_BEEP);
    spkr_on();
    printk(KERN_INFO "Start Beeping!!!\n");
    return 0;
}

static void __exit intspkr_exit(void)
{
    spkr_off();
    spkr_exit();
    printk(KERN_INFO "Stop Beeping!!!\n");

    mutex_destroy(&mutexForWriteSession);

    // baja el archivo /sys/class/intspkr/intspkr y automaticamente tambien darse baja del /dev/intspkr
    device_destroy(dev_class, devID);

    // bajar la clase de sysfs class /sys/class/intspkr
    class_destroy(dev_class);

    // baja el dispositivo intspkr
    cdev_del(&dev);

    // liberar major id and minor id
    unregister_chrdev_region(devID, 1);
    printk(KERN_INFO "Unloaded intspkr!\n");
}

module_init(intspkr_init);
module_exit(intspkr_exit);
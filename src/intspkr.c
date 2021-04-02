#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/random.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
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

/**
 * generar el número aleatorio y asociarlo con esa sesión.
 */
static int intspkr_open(struct inode *inode, struct file *filp)
{
    (*filp).private_data = kmalloc(1, GFP_KERNEL);
    get_random_bytes((*filp).private_data, 1);
    printk(KERN_ALERT "seq opened");
    return 0;
}

static ssize_t intspkr_read(struct file *filp, char __user *buf, size_t size, loff_t *f_pos)
{
    printk(KERN_ALERT "intspkr_read skipped");
    return 0;
}

static ssize_t intspkr_write(struct file *filp, const char __user *buf, size_t size, loff_t *f_pos)
{
    char soundData[4];
    size_t count = 0;
    for (; (size - count) > 4; count += 4)
    {
        if (copy_from_user(soundData, buf + count, 4))
            return -EFAULT;
        //producesSound((((uint16_t)soundData[0]) << 8) | (((uint16_t)soundData[1])), (((uint16_t)soundData[2]) << 8) | (((uint16_t)soundData[3])));
    }
    printk(KERN_ALERT "intspkr_write wrote");
    return count;
}

static int intspkr_release(struct inode *inode, struct file *filp)
{
    kfree((*filp).private_data);
    printk(KERN_ALERT "seq released");
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
static struct file_operations seq_fops = {
    .owner = THIS_MODULE,
    .open = intspkr_open,
    .release = intspkr_release,
    .write = intspkr_write,
    .read = intspkr_read,
};

static struct class *dev_class;

#define FREQUENCY_OF_BEEP 440
static int __init intspkr_init(void)
{
    /**
     * Antes de dar de alta un dispositivo de caracteres, 
     * hay que reservar sus números major y minor asociados. 
     * el major, que identifica al manejador, y el minor, 
     * que identifica al dispositivo concreto entre los que gestiona ese manejador.
     * Para esta práctica, dejaremos que el número major lo elija el propio sistema 
     * y minor como parametro de modulo
     */
    // alloc_chrdev_region devuelve un número negativo en caso de error
    if (alloc_chrdev_region(&devID, minor, 1, "intspkr") < 0)
    {
        // Si esta función, o cualquiera de las usadas en esta fase dentro de la rutina de inicio del módulo,
        // devuelve un error, esa función de inicio debe terminar retornando un -1.
        return -1;
    }

    /**
     * se debe de dar de alta dentro del núcleo la estructura de  y, 
     * especificar el conjunto de funciones de acceso que proporciona el dispositivo.
     */

    // usar la función cdev_init para dar valor inicial la estructura que representa un dispositivo de caracteres
    cdev_init(&dev, &seq_fops);
    // luego ay que asociarla con el identificador de dispositivo reservado previamente.
    cdev_add(&dev, devID, 1);

    /**
     * Aunque después de dar de alta un dispositivo dentro del núcleo ya está disponible 
     * para dar servicio a través de sus funciones de acceso exportadas, 
     * para que las aplicaciones de usuario puedan utilizarlo, 
     * es necesario crear un fichero especial de tipo dispositivo de caracteres 
     * dentro del sistema de ficheros.
     */
    // El primer paso que se debe llevar a cabo es la creación de una clase para el dispositivo gestionado por el manejador
    if ((dev_class = class_create(THIS_MODULE, "seq")) == NULL)
    {
        return -1;
    }

    // Después de crear la clase, hay que dar de alta el dispositivo asociándolo a esa clase.
    device_create(dev_class, NULL, devID, NULL, "seq");
    printk(KERN_INFO "Initialized spkr-io");

    spkr_init();
    spkr_set_frequency(FREQUENCY_OF_BEEP);
    spkr_on();
    printk(KERN_INFO "Start Beeping!!!");
    return 0;
}

static void __exit intspkr_exit(void)
{
    spkr_off();
    spkr_exit();
    printk(KERN_INFO "Stop Beeping!!!");

    device_destroy(dev_class, devID);
    class_destroy(dev_class);
    cdev_del(&dev);
    unregister_chrdev_region(devID, 1);
    printk(KERN_INFO "Unloaded spkr-io");
}

module_init(intspkr_init);
module_exit(intspkr_exit);
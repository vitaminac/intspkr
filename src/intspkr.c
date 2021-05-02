#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include "spkr-fs.h"

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
 * la estructura de datos interna que representa un dispositivo de caracteres y, 
 * dentro de esta estructura, especificar la parte más importante: 
 * el conjunto de funciones de acceso (apertura, cierre, lectura, escritura...) que proporciona el dispositivo.
 * dentro del tipo struct cdev hay un campo denominado dev de tipo dev_t 
 * que almacena el identificador de ese dispositivo
 */
static struct cdev dev;

static struct class *dev_class;

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

    init_fs();

    printk(KERN_INFO "Initialized intspkr!\n");
    return 0;
}

static void __exit intspkr_exit(void)
{
    destroy_fs();

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
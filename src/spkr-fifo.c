#include <linux/kthread.h>
#include "spkr-fifo.h"
#include "spkr-io.h"

static struct timer_list timer_list;
#define FREQUENCY_OF_BEEP 440
void playSound(struct timer_list *timer_list)
{
    spkr_set_frequency(FREQUENCY_OF_BEEP);
    spkr_on();
    printk(KERN_INFO "Start Beeping!\n");
}

static struct task_struct *consumerTask;
int consumer(void *data)
{
    printk(KERN_INFO "Starting Consumer Thread!\n");
    timer_list.expires = jiffies + msecs_to_jiffies(3000);
    add_timer(&timer_list);
    while (!kthread_should_stop())
        schedule();
    printk(KERN_INFO "Stopping spkr fifo consumer thread is created!\n");
    return 0;
}

void init_fifo(void)
{
    spkr_init();
    timer_setup(&timer_list, playSound, 0);
    printk(KERN_INFO "Creating spkr fifo consumer thread!\n");
    consumerTask = kthread_run(consumer, NULL, "spkr fifo consumer thread");
    printk(KERN_INFO "Created spkr fifo consumer thread!\n");
    printk(KERN_INFO "Initialized spkr fifo!\n");
}

void destroy_fifo(void)
{
    kthread_stop(consumerTask);
    printk(KERN_INFO "Stopped spkr fifo consumer thread!\n");
    spkr_off();
    spkr_exit();
    printk(KERN_INFO "Stop Beeping!!!\n");
    printk(KERN_INFO "Destroyed spkr fifo!\n");
}
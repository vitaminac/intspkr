#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include "spkr-fifo.h"
#include "spkr-io.h"

static struct kfifo fifo;

static wait_queue_head_t fifoNotEmptyWaitQueue;
static inline void notifyFifoIsNotEmpty(void)
{
    wake_up_interruptible(&fifoNotEmptyWaitQueue);
}
static inline void waitUntilFifoIsNotEmptyOrInterrupted(void)
{
    wait_event_interruptible(fifoNotEmptyWaitQueue, !kfifo_is_empty(&fifo) || kthread_should_stop());
}
static wait_queue_head_t fifoNotFullWaitQueue;
static inline void notifyFifoIsNotFull(void)
{
    wake_up_interruptible(&fifoNotFullWaitQueue);
}
static inline void waitUntilFifoIsNotFull(void)
{
    wait_event_interruptible(fifoNotFullWaitQueue, !kfifo_is_full(&fifo));
}
static wait_queue_head_t soundFinishPlayingWaitQueue;
static int soundFinishPlaying;
static inline void notifySoundFinishPlaying(void)
{
    soundFinishPlaying = 1;
    wake_up_interruptible(&soundFinishPlayingWaitQueue);
}
static inline void waitSoundFinishPlayingOrInterrupted(void)
{
    soundFinishPlaying = 0;
    wait_event_interruptible(soundFinishPlayingWaitQueue, soundFinishPlaying || kthread_should_stop());
}
static inline void initWaitQueues(void)
{
    init_waitqueue_head(&fifoNotEmptyWaitQueue);
    init_waitqueue_head(&fifoNotFullWaitQueue);
    init_waitqueue_head(&soundFinishPlayingWaitQueue);
}

static struct timer_list timer_list;
void wakeUpConsumerThread(struct timer_list *timer_list)
{
    notifySoundFinishPlaying();
}

typedef struct SoundData
{
    uint16_t frequency;
    uint16_t durationInMiliseconds;
} SoundData;

int consumer(void *ignore)
{
    SoundData sound;
    printk(KERN_INFO "Starting Consumer Thread!\n");
    waitUntilFifoIsNotEmptyOrInterrupted();
    while (!kthread_should_stop())
    {
        // read sound
        kfifo_out(&fifo, &sound, sizeof(sound));
        printk(KERN_INFO "Read sound with frequency %d and duration %d ms.\n", sound.frequency, sound.durationInMiliseconds);

        notifyFifoIsNotFull();
        // play sound
        spkr_set_frequency(sound.frequency);
        spkr_on();

        timer_list.expires = jiffies + msecs_to_jiffies(sound.durationInMiliseconds);
        add_timer(&timer_list);
        waitSoundFinishPlayingOrInterrupted();
        spkr_off();
        waitUntilFifoIsNotEmptyOrInterrupted();
    }
    printk(KERN_INFO "Stopping spkr fifo consumer thread!\n");
    return 0;
}

void putSound(uint16_t frequency, uint16_t durationInMiliseconds)
{
    printk(KERN_INFO "Try to add new sound with frequency %d and duration %d ms.\n", frequency, durationInMiliseconds);
    SoundData data;
    data.frequency = frequency;
    data.durationInMiliseconds = durationInMiliseconds;
    waitUntilFifoIsNotFull();
    kfifo_in(&fifo, &data, sizeof(data));
    notifyFifoIsNotEmpty();
    printk(KERN_INFO "Put new sound with frequency %d and duration %d ms.\n", frequency, durationInMiliseconds);
}

static struct task_struct *consumerTask;
#define FIFO_SIZE (256 * (sizeof(SoundData)))
void init_fifo(void)
{
    kfifo_alloc(&fifo, FIFO_SIZE, GFP_KERNEL);
    initWaitQueues();
    spkr_init();
    timer_setup(&timer_list, wakeUpConsumerThread, 0);
    printk(KERN_INFO "Created timer list.\n");
    printk(KERN_INFO "Creating spkr fifo consumer thread!\n");
    consumerTask = kthread_run(consumer, NULL, "spkr fifo consumer thread");
    printk(KERN_INFO "Created spkr fifo consumer thread!\n");
    printk(KERN_INFO "Initialized spkr fifo!\n");
}

void destroy_fifo(void)
{
    kthread_stop(consumerTask);
    printk(KERN_INFO "Stopped spkr fifo consumer thread!\n");
    del_timer_sync(&timer_list);
    printk(KERN_INFO "Removed timer list.\n");
    spkr_off();
    spkr_exit();
    kfifo_free(&fifo);
    printk(KERN_INFO "Destroyed spkr fifo!\n");
}
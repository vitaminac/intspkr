#ifndef SKPR_IO_H
#define SKPR_H
#include <linux/types.h>

#define isPC() (defined(__i386__) || defined(__x86_64__))
#define isRaspberry() __arm__

void spkr_init(void);
void spkr_exit(void);
void spkr_on(void);
void spkr_off(void);
void spkr_set_frequency(uint16_t frequency);
#endif
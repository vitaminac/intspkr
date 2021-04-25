#ifndef SKPR_IO_H
#define SKPR_IO_H
#include <linux/types.h>

void spkr_init(void);
void spkr_exit(void);
void spkr_on(void);
void spkr_off(void);
void spkr_set_frequency(uint16_t frequency);
#endif
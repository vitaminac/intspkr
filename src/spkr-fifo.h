#ifndef SKPR_FIFO_H
#define SKPR_FIFO_H

void init_fifo(void);
void destroy_fifo(void);
void putSound(uint16_t frequency, uint32_t durationInMiliseconds);

#endif
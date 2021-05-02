#ifndef SPKR_FIFO_H
#define SPKR_FIFO_H

void init_fifo(void);
void destroy_fifo(void);
void putSound(uint16_t frequency, uint16_t durationInMiliseconds);

#endif
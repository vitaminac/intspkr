#ifndef SPKR_FIFO_H
#define SPKR_FIFO_H

void init_fifo(void);
void destroy_fifo(void);
void scheduleSound(uint16_t frequency, uint16_t durationInMiliseconds);
void waitSoundFinishPlaying(void);

#endif
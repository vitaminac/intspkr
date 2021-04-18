#include "spkr-io.h"

#define isPC() (defined(__i386__) || defined(__x86_64__))
#define isRaspberry() __arm__

// PC
#if isPC()
#include <asm/io.h>
#define PROGRAMMABLE_PERIPHERAL_INTERFACE_8255_SYSTEM_CONTROL_PORT_B 0x61
/**
 * Bitfields for KB controller port B (system control port) [output]:
 * Bit(s)	Description	(Table P0392)
 * 7	    pulse to 1 for IRQ1 reset (PC,XT)
 * 6-4	    reserved
 * 3	    I/O channel parity check disable
 * 2	    RAM parity check disable
 * 1	    speaker data enable
 * 0	    timer 2 gate to speaker enable
 */
#define ENABLE_SPEAKER 0x2
#define GATE_PIT_CHANNEL_2_TO_SPEAKER 0x1
#define PIT_CHANNEL_2_ENABLED (ENABLE_SPEAKER | GATE_PIT_CHANNEL_2_TO_SPEAKER)
static void pc_spkr_on(void)
{
    uint8_t tmp = inb(PROGRAMMABLE_PERIPHERAL_INTERFACE_8255_SYSTEM_CONTROL_PORT_B) | PIT_CHANNEL_2_ENABLED;
    outb(tmp, PROGRAMMABLE_PERIPHERAL_INTERFACE_8255_SYSTEM_CONTROL_PORT_B);
}

#define PIT_CHANNEL_2_DISABLED (~PIT_CHANNEL_2_ENABLED & 0xff)
void pc_spkr_off(void)
{
    uint8_t tmp = inb(PROGRAMMABLE_PERIPHERAL_INTERFACE_8255_SYSTEM_CONTROL_PORT_B) & PIT_CHANNEL_2_DISABLED;
    outb(tmp, PROGRAMMABLE_PERIPHERAL_INTERFACE_8255_SYSTEM_CONTROL_PORT_B);
}

/*
|----------------- CONTROL REGISTER ------------------------------|
| 6 - 7          | 4 - 5       | 1 - 3          | 0               |
| SELECT CHANNEL | ACCESS MODE | OPERATING MODE | BCD/BINARY MODE |
|-----------------------------------------------------------------|

Bits         Usage
6 and 7      Select channel :
                0 0 = Channel 0 (connected with IRQ0)
                0 1 = Channel 1 (conneted with DRAM refresh)
                1 0 = Channel 2 (connected with PC Speaker)
                1 1 = Read-back command (8254 only)
4 and 5      Access mode :
                0 0 = Latch count value command
                0 1 = Access mode: lobyte only
                1 0 = Access mode: hibyte only
                1 1 = Access mode: lobyte/hibyte
1 to 3       Operating mode :
                0 0 0 = Mode 0 (interrupt on terminal count)
                0 0 1 = Mode 1 (hardware re-triggerable one-shot)
                0 1 0 = Mode 2 (rate generator)
                0 1 1 = Mode 3 (square wave generator)
                1 0 0 = Mode 4 (software triggered strobe)
                1 0 1 = Mode 5 (hardware triggered strobe)
                1 1 0 = Mode 2 (rate generator, same as 010b)
                1 1 1 = Mode 3 (square wave generator, same as 011b)
0            BCD/Binary mode: 0 = 16-bit binary, 1 = four-digit BCD
*/

#define PIT_CONTROL_REGISTER 0x43
#define SELECT_CHANNEL_2 2
#define LOW_BYTE_AND_HIGH_BYTE 3
#define SQUARE_WAVE_GENERATOR 3
#define BINARY_MODE 0
#define SEND_SQUARE_WAVE_FREQUENCY_TO_PC_SPEAKER ((SELECT_CHANNEL_2 << 6) | (LOW_BYTE_AND_HIGH_BYTE << 4) | (SQUARE_WAVE_GENERATOR << 1) | BINARY_MODE)
static inline void set_PIT_spkr_timer_square_wave_mode(void)
{
    outb(SEND_SQUARE_WAVE_FREQUENCY_TO_PC_SPEAKER, PIT_CONTROL_REGISTER);
}

#define PIT_CHANNEL_2_DATA_REGISTER 0x42
static inline void write_PIT_timer_divider(uint16_t frequency_divider)
{
    // CRITICAL SECTION: we have to write twice since the register accept only 8 bits and data is 16 bits
    outb((uint8_t)(frequency_divider), PIT_CHANNEL_2_DATA_REGISTER);
    outb((uint8_t)(frequency_divider >> 8), PIT_CHANNEL_2_DATA_REGISTER);
}

#include <linux/timex.h> // PIT_TICK_RATE
// spin_lock_irqsave and spin_unlock_irqrestore
#include <linux/spinlock.h>
#include <linux/i8253.h> // i8253_lock
static inline void pc_spkr_set_frequency(uint16_t frequency)
{
    uint16_t frequency_divider = PIT_TICK_RATE / frequency;
    unsigned long flags;
    raw_spin_lock_irqsave(&i8253_lock, flags);
    set_PIT_spkr_timer_square_wave_mode();
    write_PIT_timer_divider(frequency_divider);
    raw_spin_unlock_irqrestore(&i8253_lock, flags);
}
#endif

// Raspberry Pi
#ifdef isRaspberry()
#endif

void spkr_init(void)
{
}

void spkr_exit(void)
{
}

void spkr_on(void)
{
#if isPC()
    pc_spkr_on();
#endif
}

void spkr_off(void)
{
#if isPC()
    pc_spkr_off();
#endif
}

void spkr_set_frequency(uint16_t frequency)
{
#if isPC()
    pc_spkr_set_frequency(frequency);
#endif
#ifdef isRaspberry()
#endif
}
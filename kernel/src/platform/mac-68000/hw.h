#pragma once

#include <stdint.h>

// VIA base address
#define MAC_VIA_BASE        0xefe1fe

// VIA register addresses
#define MAC_VIA_BUFB        (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 0)))   // register B (zero offset)
#define MAC_VIA_DDRB        (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 2)))   // register B direction register
#define MAC_VIA_DDRA        (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 3)))   // register A direction register
#define MAC_VIA_T1C_L       (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 4)))   // timer 1 counter (low-order byte)
#define MAC_VIA_T1C_H       (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 5)))   // timer 1 counter (high-order byte)
#define MAC_VIA_T1L_L       (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 6)))   // timer 1 latch (low-order byte)
#define MAC_VIA_T1L_H       (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 7)))   // timer 1 latch (high-order byte)
#define MAC_VIA_T2C_L       (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 8)))   // timer 2 counter (low-order byte)
#define MAC_VIA_T2C_H       (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 9)))   // timer 2 counter (high-order byte)
#define MAC_VIA_SR          (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 10)))  // shift register (keyboard)
#define MAC_VIA_ACR         (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 11)))  // auxiliary control register
#define MAC_VIA_PCR         (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 12)))  // peripheral control register
#define MAC_VIA_IFR         (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 13)))  // interrupt flag register
#define MAC_VIA_IER         (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 14)))  // interrupt enable register
#define MAC_VIA_BUFA        (*((volatile uint8_t *) (MAC_VIA_BASE + 512 * 15)))  // register A

// VIA register A constants
#define MAC_VIA_REG_A_OUT   0x7f    // direction register A:  1 bits = outputs
#define MAC_VIA_REG_A_INIT  0x7b    // initial value for MAC_VIA_BUF_A (medium volume)
#define MAC_VIA_VOL_SOUND   0x7     // sound volume bits

// VIA register A bit numbers
#define MAC_VIA_SOUND_PG2   0x3     // 0 = alternate sound buffer
#define MAC_VIA_ROM_OVERLAY 0x4     // 1 = ROM overlay (system startup only)
#define MAC_VIA_HEAD_SEL    0x5     // disk SEL control line
#define MAC_VIA_SCREEN_PG2  0x6     // 0 = alternate screen buffer
#define MAC_VIA_SCC_WREQ    0x7     // SCC wait/request line

// VIA register B constants
#define MAC_VIA_REG_B_OUT   0x87    // direction register B:  1 bits = outputs
#define MAC_VIA_REG_B_INIT  0x07    // initial value for MAC_VIA_BUF_B

// VIA interrupt bits
#define MAC_VIA_INT_IRQ     0x80    // IRQ (all enabled VIA interrupts)
#define MAC_VIA_INT_TIMER1  0x40    // Timer 1
#define MAC_VIA_INT_TIMER2  0x20    // Timer 2
#define MAC_VIA_INT_KBCLK   0x10    // Keyboard clock
#define MAC_VIA_INT_KBDAT   0x8     // Keyboard data bit
#define MAC_VIA_INT_KBRDY   0x4     // Keyboard data ready
#define MAC_VIA_INT_VBLANK  0x2     // Vertical blanking interrupt
#define MAC_VIA_INT_1SEC    0x1     // One-second interrupt

// keyboard commands
#define MAC_KEYB_INQ        0x10    // Inquiry
#define MAC_KEYB_INST       0x14    // Instant
#define MAC_KEYB_MODEL      0x16    // Model Number
#define MAC_KEYB_TEST       0x36    // Test

// null response
#define MAC_KEYB_NULL       0x7b
#define MAC_KEYB_ACK        0x7d
#define MAC_KEYB_NACK       0x77

// SCC
#define MAC_SCC_RD_BASE     0x9ffff8
#define MAC_SCC_WR_BASE     0xbffff9

#define MAC_SCC_A_DATA_RD   (*((volatile uint8_t *) (MAC_SCC_RD_BASE + 0x6))) // SCC channel A data read
#define MAC_SCC_A_DATA_WR   (*((volatile uint8_t *) (MAC_SCC_WR_BASE + 0x6))) // SCC channel A data write
#define MAC_SCC_A_CTL_RD    (*((volatile uint8_t *) (MAC_SCC_RD_BASE + 0x2))) // SCC channel A control read
#define MAC_SCC_A_CTL_WR    (*((volatile uint8_t *) (MAC_SCC_WR_BASE + 0x2))) // SCC channel A control write
#define MAC_SCC_B_DATA_RD   (*((volatile uint8_t *) (MAC_SCC_RD_BASE + 0x4))) // SCC channel B data read
#define MAC_SCC_B_DATA_WR   (*((volatile uint8_t *) (MAC_SCC_WR_BASE + 0x4))) // SCC channel B data write
#define MAC_SCC_B_CTL_RD    (*((volatile uint8_t *) (MAC_SCC_RD_BASE + 0x0))) // SCC channel B control read
#define MAC_SCC_B_CTL_WR    (*((volatile uint8_t *) (MAC_SCC_WR_BASE + 0x0))) // SCC channel B control write

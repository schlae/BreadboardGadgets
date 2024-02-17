// Fuses E:F7, H:D9, L:E2
#define F_CPU (8000000UL)
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <stdbool.h>

#define ANODE1 5
#define ANODE2 4
#define C_A 3 // PB
#define C_D 2 // PB
#define C_E 1 // PB
#define C_F 0 // PB

#define C_C 3 // PC
#define C_G 2 // PC
#define C_DP 1 // PC
#define C_B 0 // PC

#define CONFIG_SINGLE 6 // PB
#define CONFIG_EDGE 0 // PE
#define CONFIG_FALL 1 // PE
#define UPSIDE 2 // PE
#define LATCH 3 // PE

//   AAA       DDD
//  F   B     C   E
//   GGG       GGG
//  E   C     B   F
//   DDD       AAA
// 0000 0000 -> CG.B ADEF
// 0 = 1001 1111
// 1 = 1001 0000
// 2 = 0101 1110
// 3 = 1101 1100
// 4 = 1101 0001
// 5 = 1100 1101
// 6 = 1100 1111
// 7 = 1001 1000
// 8 = 1101 1111
// 9 = 1101 1101
// A = 1101 1011
// B = 1100 0111
// C = 0000 1111
// D = 1101 0110
// E = 0100 1111
// F = 0100 1011

uint8_t digit_table[] = { 0x9F, 0x90, 0x5E, 0xDC,
                          0xD1, 0xCD, 0xCF, 0x98,
                          0xDF, 0xDD, 0xDB, 0xC7,
                          0x0F, 0xD6, 0x4F, 0x4B };

volatile uint8_t current_data;
volatile bool triggered = false;
volatile uint8_t dpoints = 0;
volatile uint8_t trig_oneshot = 0;

// Pin change interrupt for trigger/latch input
ISR (PCINT3_vect)
{
    bool pin_state = PINE & _BV(LATCH);

    // Check rising vs falling
    if ((pin_state == !(PINE & _BV(CONFIG_FALL))) & !triggered) {
        current_data = PIND;
        if (PINB & _BV(CONFIG_SINGLE)) {
            triggered = true;
        } else {
            trig_oneshot = 1;
        }
        dpoints |= 1; // Turn on final DP
    }
}

// Puts a digit on the 7-segment display
void load_digit(uint8_t val, uint8_t anode)
{
    uint8_t d = ~digit_table[val]; // Invert bits since LED on when 0
    uint8_t t;
    uint8_t dpoint;

// 0000 0000 -> CG.B ADEF
    if (PINE & _BV(UPSIDE)) {
        // Swap digits around
        // Swap A,D(2,3); B,E(4,1); F,C(0,7)
        t = (_BV(2) & d) << 1;
        t |= (_BV(3) & d) >> 1;
        t |= (_BV(4) & d) >> 3;
        t |= (_BV(1) & d) << 3;
        t |= (_BV(7) & d) >> 7;
        t |= (_BV(0) & d) << 7;
        t |= (_BV(5) & d);
        t |= (_BV(6) & d);
        d = t;
        anode = (anode == 2) ? 1 : 2;
    }

    dpoint = (dpoints >> (anode - 1)) & 1;

    PORTB = d & 0x0F;
    PORTC = ((anode << 4) | (d >> 4)) & ~(dpoint << 1);
}


int main (void)
{
    bool pin_state;
    // Set up IO ports
    DDRD = 0x00; // Binary 8-bit input port
    DDRB = 0x0F; // Output for LED cathode drivers
    PORTB = 0xC0; // Pullup on PB6
    DDRC = 0x3F; // Output for LED cathode and anode drivers
    DDRE = 0x00; // Config and trigger input
    PORTE = 0x0F; // Pullups on PE0, 1, 2, 3

    // Set up PCINT27 as interrupt on pin change
    PCMSK3 = _BV(PCINT27);
    PCICR = _BV (PCIE3);
    sei();
    while (1) {
        // Transparent latch
        if (!(PINE & _BV(CONFIG_EDGE))) {
            pin_state = PINE & _BV(LATCH);
            if ((pin_state == !(PINE & _BV(CONFIG_FALL))) && !triggered) {
                current_data = PIND;
                dpoints |= _BV(0);
                if (PINB & _BV(CONFIG_SINGLE)) {
                    triggered = true;
                } else {
                    trig_oneshot = 1;
                }
            }
        }

        // Check for normal mode
        if (!(PINB & _BV(CONFIG_SINGLE))) {
            if (triggered) {
                triggered = false;
                dpoints &= ~_BV(0); // Turn off final DP
            }
        }


        // Check for trigger one-shot
        if (trig_oneshot > 0) {
            trig_oneshot++;
            if (trig_oneshot > 100) {
                dpoints &= ~_BV(0); // Turn off final DP
                trig_oneshot = 0;
            }
        }

        load_digit(current_data & 0x0F, 1);
        _delay_ms(1);
        load_digit(current_data >> 4, 2);
        _delay_ms(1);
    }
    return 0;
}

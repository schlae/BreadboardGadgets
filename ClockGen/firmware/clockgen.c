// Fuses E:F7, H:D9, L:E2
#define F_CPU (8000000UL)
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

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

// Averaging filter for removing ADC noise
#define FILT_BITS (5)    // Number of fixed point fractional bits
#define AVG_FILT(input, output)  output = (output - (output >> FILT_BITS) + (input))
#define GET_AVG(v) ((v + _BV(FILT_BITS) / 2) >> FILT_BITS) // Rounds up

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

uint8_t dpoints = 0;

uint8_t adc_chan = 0;
uint16_t val_duty = 0;
uint16_t val_freq = 0;
volatile uint16_t val_vref = 0;
uint8_t val_range = 0;

volatile uint16_t val_actual_freq = 0;
volatile uint16_t val_actual_duty = 0;
volatile uint16_t val_actual_duty_old = 0;
uint8_t disp_freq = 0;
uint8_t disp_duty = 0;

#define DUTY_DISP_TIME 200
volatile uint16_t duty_disp_timer = 0;

#define BANDGAP_VOLTAGE (1.1)
#define VCC_THRESHOLD (4.5)
#define VREF_TH ((int)(255.0 * BANDGAP_VOLTAGE / VCC_THRESHOLD))
#define RESET_DELAY 250
uint16_t reset_delay = 0;

// Get the position of the frequency range switch
uint8_t get_range()
{
    uint8_t st;
    st = ((PINB & 0xC0) >> 6);
    st |= (PINE & 0x03) << 2;
    switch (st) {
        case 0xE:
            val_range = 0;
            break;
        case 0xD:
            val_range = 1;
            break;
        case 0xB:
            val_range = 2;
            break;
        case 0x7:
            val_range = 3;
            break;
        default:
            // Physical rotary switch goes to 0xF in between detents
            // So just ignore that and use the previous setting.
            break;
    }
    return val_range;
}

// Puts a digit on the 7-segment display
void load_digit(uint8_t val, uint8_t anode)
{
    uint8_t d = ~digit_table[val]; // Invert bits since LED on when 0
    uint8_t dpoint;

    dpoint = (dpoints >> (anode - 1)) & 1;

    PORTB = d & 0x0F;
    PORTC = ((anode << 4) | (d >> 4)) & ~(dpoint << 1);
}

// Hacky binary to BCD converter
uint8_t bin2bcd(uint8_t val)
{
    char c[4];
    itoa(val, c, 10);
    if (val < 10) {
        return c[0] - 0x30;
    } else {
        return (c[1] - 0x30) | ((c[0] - 0x30) << 4);
    }
}

// Set the ADC channel mux
void adc_set_chan(uint8_t chan)
{
    ADMUX = (ADMUX & 0xF0) | (chan & 0xf);
    adc_chan = chan;
}

// Convert all ADC channels and filter results
ISR(ADC_vect)
{
    switch (adc_chan) {
        case 0x6: // Duty cycle
            AVG_FILT(255 - ADCH, val_duty);
            adc_set_chan(0x7);
            val_actual_duty = (GET_AVG(val_duty));
            if (val_actual_duty != val_actual_duty_old) {
                duty_disp_timer = DUTY_DISP_TIME;
                val_actual_duty_old = val_actual_duty;
            }
            break;
        case 0x7: // Frequency
            AVG_FILT(255 - ADCH, val_freq);
            adc_set_chan(0xe);
            // Ensure there is no 0, since 0 Hz makes no sense
            val_actual_freq = ((GET_AVG(val_freq) * 99L) >> 8) + 1;
            break;
        default:
        case 0xe: // Internal bandgap reference voltage
            AVG_FILT(ADCH, val_vref);
            adc_set_chan(0x6);
            break;
    }
    _delay_us(100);
    ADCSRA |= _BV(ADSC);
}

// Handle reset pulse generation and timing
void do_supervisor()
{
    uint16_t vref = GET_AVG(val_vref);
    if (vref < VREF_TH) {
        //  Exit reset
        reset_delay++;
        if (reset_delay > RESET_DELAY) {
            reset_delay = RESET_DELAY;
            PORTD = 0;
            dpoints = 0;
        }
    } else {
        //  Enter reset
        PORTD = _BV(1) | _BV(3);
        dpoints = 1;
    }

}

// Prescaler ranges are 256, 8, 1, 1
uint8_t ranges[] = {4, 2, 1, 1};
uint32_t prescale[] = {256, 8, 1, 1};
uint32_t frange[] = {1, 100, 1000, 10000};

// Calculate PWM parameters and update hardware timer
void update_timer()
{
    uint32_t freq;
    uint32_t regval;

    // Calculate desired register value
    freq = (uint32_t)val_actual_freq * frange[val_range] * prescale[val_range];
    regval = (F_CPU / freq) - 1;
    if (regval > 65535) regval = 65535;

    // Display the real value
    disp_freq = (F_CPU / (prescale[val_range] * (regval + 1))) / frange[val_range];

    // Setup prescaler
    TCCR3B = (TCCR3B & 0xF8) | ranges[val_range];
    ICR3 = regval;
    OCR3A = regval * val_actual_duty / 256;

    // Displayed duty cycle
    disp_duty = ((OCR3A + 1) * 100L) / (regval + 1);
}

int main (void)
{
    uint16_t displayval;

    // Set up IO ports
    DDRD = 0x0B; // Output for clk, reset, resetdrv. In for resetsense
    PORTD = 0x0A; // RESET is high, RESET# is low
    DDRB = 0x0F; // Output for LED cathode drivers
    PORTB = 0xC0; // Pullup on PB6, PB7
    DDRC = 0x3F; // Output for LED cathode and anode drivers
    DDRE = 0x00; // Config inputs, ADC inputs
    PORTE = 0x03; // Pullups on PE0, 1

    // Setup ADC
    ADMUX = _BV(REFS0) | _BV(ADLAR); // Left adjust results, Vref=VCC.
    ADCSRA = _BV(ADEN) | _BV(ADIE) | 7; // Division factor of 128
    adc_set_chan(6);
    sei();
    ADCSRA |= _BV(ADSC);

    // Setup timer
    TCCR3A = _BV(COM3A1) | _BV(WGM31);
    TCCR3B = _BV(WGM33) | _BV(WGM32);

    dpoints = 1;

    while (1) {
        get_range();
        update_timer();
        do_supervisor();

        if (duty_disp_timer > 0) {
            duty_disp_timer--;
            displayval = bin2bcd(disp_duty);
        } else {
            displayval = bin2bcd(disp_freq);
        }
        load_digit(displayval & 0x0F, 1);
        _delay_ms(1);
        load_digit(displayval >> 4, 2);
        _delay_ms(1);
    }
    return 0;
}

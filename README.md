# Breadboard Gadgets

Here are some handy little boards that plug directly into a solderless
breadboard and help you with common functions.

* ByteHex: Examine an 8-bit data bus, in hex, with numerous latching options.
* ByteDisplay: Similar to above but with 8 discrete LEDs. No microcontroller.
* ByteSwitch: 8-bit DIP switch with pull-up/down resistors.
* ClockGen: A pulse generator that produces a clock up to 1MHz with an adjustable duty cycle. Generates RESET pulses as well.


## Building Firmware
The ByteHex and ClockGen boards have an ATMega328PB microcontroller. The code is meant to run without a bootloader and can be built from the command line if you have installed avr-gcc and avrdude.

```
make
make fuses
make prog
```

Note that the avrdude command lines in the makefile are hard-coded for the usbtiny programmer. You'll want to change that if you have another type of AVR programmer.

The MCU is new enough that your version of avr-gcc and avrdude may not
support it yet, see [here](https://gist.github.com/goncalor/51e1c8038cc058b4379552477255b4e1) for instructions on how to fix that.

The bottom of each board has a standard 6-pin AVR programming header as a surface mount footprint. Since you only need to program it once, you could plug a header into the programmer's cable and physically push it up against the board, or you could temporarily tack-solder it in place. You can't really leave the header in place because it may interfere mechanically when you try to plug the board into a breadboard.

## Using the boards

### ClockGen

Adjust the frequency with the upper potentiometer. The lower potentiometer sets the duty cycle. When you set the duty cycle, the percent value will temporarily appear on the hex display.

The frequency range is set by the knob marked 0, 1, 2, and 3. These settings correspond with the following ranges:

| Range | Frequencies      |
|-------|------------------|
|   0   | 1 to 99Hz        |
|   1   | 100Hz to 999Hz   |
|   2   | 1KHz to 99KHz    |
|   3   | 100KHz to 999KHz |

The rightmost decimal point will light up when the board is generating a reset pulse.

### ByteHex

Plug the trigger signal into the TRIG jack on the board, or leave it unconnected and set switch 3 to L to display the instantaneous digital value.

There are four configuration switches:

| Switch    | Function            |
|-----------|---------------------|
|   1 (6/9) | Right-side up (up) or upside-down (down) display |
|   2 (R/F) | Trigger on rising (up) or falling (down) edge    | 
|   3 (L/E) | Level sensitive (up) or edge-triggered (down)    | 
|   4 (N/S) | Normal mode (up) or single-shot trigger mode (down) |

The upside-down display is useful if you plug the board into the left side of a breadboard.

The level sensitive mode turns the device into a transparent latch when the trigger input is high (R) or low (F), depending on switch 2.

Single-shot trigger mode captures a single trigger event, freezing the display until you cycle power or change the trigger mode back to normal. This lets you capture a single data byte, something like a simple digital oscilloscope.

# Licensing

This work is licensed under a Creative Commons Attribution-ShareAlike 4.0 International License. See [https://creativecommons.org/licenses/by-sa/4.0/](https://creativecommons.org/licenses/by-sa/4.0/).

# C-Cubed Firmware

## A sketch for ATmega-328 based Arduinos

### Description
This firmware, the accompanying KiCad files, and documentation, are for
implementing a combination clock, calendar, and calculator device that
displays numeric information via nixie tubes.

### In-depth Description

#### Hardware
In particular, an Arduino Nano controlls two 4051 multiplexers, one each
for clock and reset signals, which interface with individual 4017 decoded
Johnson counters, which have high-voltage TN5325 MOSFETs on each output,
so that one digit at a time can be lit up per IN-12 nixie tube.

Interaction with the device is made possible by a collection of push
button switches that are hooked up to two additional 4051 multiplexers,
that act as a single 1-to-16 MUX; The common line back to the Arduino is
configured as an `INPUT_PULLUP`, so whenever a button is pushed, the
line is pulled low, indicating a press to the Arduino. Optionally, two
4094 shift registers can be used to control status LEDs and the decimal
points on the IN-12B revision nixie tubes.

#### Firmware
As for the particulars of the firmware, an `unsigned long`, or a 32-bit
unsigned integer is used to store the number of hundredths of seconds
passed on the current day. This value, called the `timeUnit`, is
incremented every 10 milliseconds by a timer interrupt in the Arduino,
and is as accurate as possible for time-keeping on such a device.
The `timeUnit` is read from and used to derive the hours, minutes, and
seconds of the day, and when it overflows, the date information is
incremented according to the standards of the Gregorian calendar.

#### Calculator
The calculator portion of the device is only a four-function machine,
and only has two "registers". Once a first number is typed and then
a operation button is pressed, the first number is loaded in the
"working" register. Then, once a second number is typed and the equals
button or anoter opeation button is pressed, the second number is loaded
into the "auxiliary" register, and the first operation is carried out.
The result is then deposited into the "working" register, and the next
operation is prompted.

For example, typing in `2 + 2 + 2 + 2 + 2 =` would yield `10`, but show
the display counting up `4`, `6`, `8`, on every press of `+` after the
first one.

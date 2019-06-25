/*
 * C-Cubed: A combination Clock, Calendar, and Calculator
 * Written by Charles "Charlie" "The Gnarly" Cook
 *
 * This program is designed for an Arduino Uno and/or an ATmega 328P
 * microcontroller. It configures Timer1 to act as a 100Hz square-wave
 * generator so that the clock component can be accurate to hundredths
 * of seconds. Up to eight (8) display digits can be hooked up via pins
 * 4 through 8, which assumes there is a pair of analog multiplexers
 * controlling the digits, which should individually be driven by
 * decoded decade counters. It is meant by control that the decade
 * counters can be either clocked or reset by the controller one at
 * a time.
 *
 * By this abstraction with the counters, the display digits can be
 * either homebrew LED displays, or more preferably, Nixie Tubes.
 * The general idea of displaying a number involves resetting all
 * counters, then pulsing all the counters however many times is equal
 * to the digit to be displayed. One limitation this causes is that no
 * digit display can be disabled, and thus must always show some digit.
 *
 * Back to the clock. As it's smallest unit of time is a hundredth of
 * a second, of which there are 6,000 in a minute, a bit more math
 * (60 * 24 = 1440 minutes in a day) shows that the time of day can be
 * held in the controller as a number between 0 and 8,639,999;
 * The prior number representing 0:00:00.00 (midnight) and the later
 * representing 23:59:59.99 (right before midnight). To acommodate this
 * time value, an `unsigned long` must be used.
 *
 * From this time value, updating the calendar date is trivial; simply
 * do so when the time value overflows to 8,640,000, then keep in mind
 * how leap years work. If using all eight (8) display digits, the
 * month, day, and full year can be displayed (if using the Gregorian
 * system and if it is not the 11th millennia).
 *
 * As for the calculator, it is designed for just four functions:
 * addition, subtraction, multiplication, and division. This is because
 * pins 9 through 13 are designed for a 16 port analog multiplexor;
 * Thus there are 16 buttons to cycle through and pass back to pin 13,
 * which is a pulled-up input pin. If there are ten (10) digit buttons,
 * one (1) equals-sign button, and one (1) clear button, that leaves
 * four (4) function buttons.
 *
 * Back to the buttons: The address lines to the multiplexor will cycle
 * through the buttons once every Timer1 interrupt (once every 10ms). If
 * pin 13 is read to be LOW, that means the current button is pressed,
 * as the outer side of all buttons is ground, and pin 13 is a pulled-up
 * input pin. The clear button will also act as a mode-changing button;
 * When heled for half of a second (50 interrupts), the controller will
 * cycle through from clock mode, to calendar mode, to calculator mode,
 * then back again.
*/

// I/O Pin Definitions
#define RESCOM 4 // Reset Common
#define CLKCOM 5 // Clock Common
#define NDADR0 6 // Nixie Driver Address Bit 0
#define NDADR1 7 // ... 1
#define NDADR2 8 // ... 2
#define BTNCOM 9 // Button Common
#define BTADR0 10 // Button Address Bit 0
#define BTADR1 11 // ... 1
#define BTADR2 12 // ... 2
#define BTADR3 13 // ... 3

// Mode state definitions
#define MODECLOCK = 2
#define MODECALEN = 4
#define MODECALCU = 8

// Multiplexer Address AND Bit Masks
#define MXMSK0 1
#define MXMKS1 2
#define MXMSK2 4
#define MXMSK3 8

// Nixie Digit Addresses
#define HUNDRL 0
#define HUNDRH 1
#define SECONL 2
#define SECONH 3
#define MINUTL 4
#define MINUTH 5
#define HOURSL 6
#define HOURSH 7

// Old Time Addresses
#define OLDHUN 0
#define OLDSEC 1
#define OLDMIN 2
#define OLDHOR 3

// Global Constants and Variables
const int timer1Comparison = 624;
const int timer1Cycles = 50;
const unsigned long timeMidnight 8640000;
const byte[12] daysInAMonth = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
unsigned long timeUnit = 0;
byte calendarDay = 1;
byte calendarMonth = 1;
unsigned int calendarYear 1970;
byte[4] oldTimes;
bool timeChanged = false;
bool dateChanged = false;
bool modeChanged = false;
bool rolloverSec = false;
bool rolloverMin = false;
bool rolloverHor = false;
byte mode = MODECLOCK;
byte tempByte;

// Function Prototypes
byte getHours(unsigned long time);
byte getMinutes(unsigned long time);
byte getSeconds(unsigned long time);
byte getHundredths(unsigned long time);
byte getHighDigit(byte value);
byte getLowDigit(byte value);
void pulsePin(byte pin);
void addressNixieMux(byte address);
void addressButtonMux(byte address);
void clearDigit(byte digit);
void setDigit(byte value, byte digit);
void setTimeFlags(unsigned long time);
void pushOldTimes(unsigned long time);
bool isLeapYear(unsigned int year);
bool isDayOverflowed(byte day, byte month, unsigned long year);

// Setup Function
void setup() {
	// Set pinModes
	pinMode(RESCOM, OUTPUT);
	pinMode(CLKCOM, OUTPUT);
	pinMode(NDADR0, OUTPUT);
	pinMode(NDADR1, OUTPUT);
	pinMode(NDADR2, OUTPUT);
	pinMode(BTNCOM, INPUT_PULLUP);
	pinMode(BTADR0, OUTPUT);
	pinMode(BTADR1, OUTPUT);
	pinMode(BTADR2, OUTPUT);
	pinMode(BTADR3, OUTPUT);

	// Setup Timer1 as main clock signal generator
	noInterrupts();
	// The below register configurations set Timer1 to produce an interrupt every 10ms
	TCCR1A = 0; // Clear any preset configurations in the first config register for Timer1
	TCCR1B |= 0x04; // Set Bit 2 of the second config register for Timer1
	TCCR1B &= 0xFC; // Clear Bits 1 and 0 of the second config register for Timer1
	// Overall, TCCR1B[2:0] = 0b100, which enables a prescaler of x256.
	// This means Timer1 will operate at 62.5kHz, thus it will have a 16us period.
	TCNT1 = 0; // Reset Timer1
	OCR1A = timer1Comparison; // Set Timer1 to generate an interrupt when it equals timer1Comparison
	// By default, if timer1Comparison is 624, then an interrupt will occur every (625 * 16)us = 10,000us = 10ms.
	// I swapped to 625 in the calculation because the counter starts at 0, meaning there are 625 cycles from
	// 0 to 624 inclusive. It's weird, but my scope shows this makes for a more accurate waveform frequency-wise.
	TIMSK1 = 0x02; // Set only Bit 1 of the Timer1 Mask register so only the OCR1A interrupt will be generated
	// Reset all digits
	for (tempByte = 0; tempByte < 8; tempByte++) {
		clearDigit(tempByte);
	}
	// Re-enable interrupts to start the clock
	interrupts();
}

// Interrupt Service Routine called when Timer1 generates a Compare-Match interrupt (from register A)
ISR(TIMER1_COMPA_vect) {
	TCNT1 = 0; // Reset Timer1
	timeUnit = (timeUnit + 1) % timeMidnight; // Increment the time unit counter
	timeChanged = true; // Set the time change flag
}

// Main Loop
void loop() {
	if (timeChanged) {
		setTimeFlags(timeUnit);
		addressNixieMux(HUNDRL);
		pulsePin(CLKCOM);
		if (rolloverSec) {
			clearDigit(SECONH);
			addressNixieMux(MINUTL);
			pulsePin(CLKCOM);
			rolloverSec = false;
			if (rolloverMin) {
				clearDigit(MINUTH);
				tempByte = getHours(timeUnit);
				setDigit(getLowDigit(tempByte), HOURSL);
				setDigit(getHighDigit(tempByte), HOURSH);
				rolloverMin = false;
				if (rolloverHor) {
					clearDigit(HOURSH);
					clearDigit(HOURSL);
					dateChanged = true;
				}
			}
		}
		pushOldTimes(timeUnit);
		timeChanged = false;
	}
	if (dateChanged) {
		calendarDay++;
		if (isDayOverflowed(calendarDay, calendarMonth, calendarYear)) {
			calendarDay = 1;
			calendarMonth++;
			if (calendarMonth == 13) {
				calendarMonth = 1;
				calendarYear++;
			}
		}
		dateChanged = false;
	}
	switch (mode) {
		case MODECLOCK:
			if (modeChanged) {
			} 
			break;
		case MODECALEN:
			if (modeChanged) {
			}
			break;
		case MODECALCU:
			if (modeChanged) {
			}
			break;
	}
}

// Function Bodies
// Hours Getter Function
byte getHours(unsigned long time) {
	return (byte) (time / 360000);
}

// Minutes Getter Function
byte getMinutes(unsigned long time) {
	return (byte) ((time % 360000) / 6000);
}

// Seconds Getter Function
byte getSeconds(unsigned long time) {
	return (byte) ((time % 6000) / 100);
}

// Hundredths-Seconds Getter Function
byte getHundredths(unsigned long time) {
	return (byte) (time % 100);
}

// Digit Getter Functions
byte getHighDigit(byte value) {
	return (value % 100) / 10;
}
byte getLowDigit(byte value) {
	return value % 10;
}

// Pin Pulse Function (t_w = 1us, DC% = 50)
void pulsePin(byte pin) {
	digitalWrite(pin, HIGH);
	delayMicroseconds(1);
	digitalWrite(pin, LOW);
	delayMicroseconds(1);
}

// Multiplexer Addressing Functions
void addressNixieMux(byte address) {
	address & MXMSK0 ? digitalWrite(NDADR0, HIGH) : digitalWrite(NDADR0, LOW);
	address & MXMSK1 ? digitalWrite(NDADR1, HIGH) : digitalWrite(NDADR1, LOW);
	address & MXMSK2 ? digitalWrite(NDADR2, HIGH) : digitalWrite(NDADR2, LOW);
}
void addressButtonMux(byte address) {
	address & MXMSK0 ? digitalWrite(BTADR0, HIGH) : digitalWrite(BTADR0, LOW);
	address & MXMSK1 ? digitalWrite(BTADR1, HIGH) : digitalWrite(BTADR1, LOW);
	address & MXMSK2 ? digitalWrite(BTADR2, HIGH) : digitalWrite(BTADR2, LOW);
	address & MXMSK3 ? digitalWrite(BTADR3, HIGH) : digitalWrite(BTADR3, LOW);
}

// Digit Reset Wrapper
void clearDigit(byte digit) {
	addressNixieMux(digit);
	pulsePin(RESCOM);
}

// Digit Setter Function
void setDigit(byte value, byte digit) {
	clearDigit(digit);
	int i;
	for (i = 0; i < value; i++) {
		pulsePin(CLKCOM);
	}
}

// Time Rollover Detection Function
void setTimeFlags(unsigned long time) {
	getSeconds(time) < oldTimes[OLDSEC] ?
		rolloverSec = true :
		rolloverSec = false;
	if (!rolloverSec) return;

	getMinutes(time) < oldTimes[OLDMIN] ?
		rolloverMin = true :
		rolloverMin = false;
	if (!rolloverMin) return;

	getHours(time) < oldTimes[OLDHOR] ?
		rolloverHor = true :
		rolloverHor = false;
}

// Old Times Push Function
void pushOldTimes(unsigned long time) {
	oldTimes[OLDHUN] = getHundredths(time);
	oldTimes[OLDSEC] = getSeconds(time);
	oldTimes[OLDMIN] = getMinutes(time);
	oldTimes[OLDHOR] = getHours(time);
}

bool isLeapYear(unsigned int year) {
	return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

bool isDayOverflowed(byte day, byte month, unsigned long year) {
	bool standardDayOverflow = day > daysInAMonth[month];
	if (isLeapYear(year)) {
		if (month == 2) {
			return days > 29;
		} else {
			return standardDayOverflow;
		}
	} else {
		return standardDayOverflow;
	}
}

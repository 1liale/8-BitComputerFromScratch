#define A0 0
#define A1 1
#define A2 2
#define A3 3
#define A4 4
#define A5 5
#define A6 6
#define A7 21
#define A8 22
#define A9 23
#define A10 24

#define IO0 12
#define IO1 13
#define IO2 14
#define IO3 25
#define IO4 10
#define IO5 11
#define IO6 26
#define IO7 27
#define WE 28
#define OE 29

#include <stdio.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <iostream>
#include <wiringPi.h>

using namespace std;
using namespace std::this_thread;
using namespace std::chrono;

// IO0 is least sig fig, IO7 is most sig fig
const int ioPins[] = {IO0, IO1, IO2, IO3, IO4, IO5, IO6, IO7};
const int ioPinsLength = 8;
const int MAX_VAL = (1 << ioPinsLength) - 1;

const int addrPins[] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10};
const int addrPinsLength = 11;
const int MAX_ADDR = (1 << addrPinsLength) - 1 ;

// Set addr pins to an address
void setAddr(int address) {
	if (address > MAX_ADDR) {
		printf("Invalid address %d\n", address);
		return;
	}

	for (int pin = 0; pin < addrPinsLength; ++pin) {
		pinMode(addrPins[pin], OUTPUT);
		digitalWrite(addrPins[pin], address & 1);
		address = address >> 1;
	}
}

// Send val to an address
void writeToMem(int address, int val) {
	if (val > MAX_VAL) {
		printf("Invalid value %d\n", val);
		return;
	}
	
	setAddr(address);
	digitalWrite(OE, HIGH);
	
	for (int pin = 0; pin < ioPinsLength; ++pin) {
		pinMode(ioPins[pin], OUTPUT); 
		digitalWrite(ioPins[pin], val & 1);
		val = val >> 1;
	}
		
	sleep_for(nanoseconds(60)); // Satisfy OE and addr pins setup time
	digitalWrite(WE, LOW);
	sleep_for(nanoseconds(100)); // Satisfy WE pulse width
	digitalWrite(WE, HIGH);
	sleep_for(nanoseconds(10)); // Satisfy data pins hold time
}

// Read in val from an address
int readFromMem(int address) {	
	setAddr(address);
	digitalWrite(OE, LOW);
	
	for (int pin = 0; pin < ioPinsLength; ++pin) {
		pinMode(ioPins[pin], INPUT);
	}
	sleep_for(nanoseconds(150)); // Satisfy output delays
	
	int data = 0;
	for (int pin = 0; pin < ioPinsLength; ++pin) {
		data += digitalRead(ioPins[pin]) << pin;
	}
	return data;
}

// Print n qwords from memory
void printMem(int qwords) {
	for (int qword = 0; qword < qwords; ++qword) {
		    int data[16];
		    int byte = qword << 4;
		    for (int offset = 0; offset <= 15; ++offset) {
		    	data[offset] = readFromMem(byte + offset); 
		    }

		    char buf[80];
       	            sprintf(buf, "%03x:  %02x %02x %02x %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x %02x %02x",
						            byte, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
							    data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

		    printf("%s\n", buf);
	}
}

// Set all memory address to val.
void clearMem(int val) {
	for (int addr = 0; addr <= MAX_ADDR; ++addr) {
		writeToMem(addr, val); 
	}
}

// Program eeprom for display on 7 segment leds
int programDisplay() {
	int digits[] = { 0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b };
	int mem[MAX_ADDR + 1];

	// Program unsigned int display
	for (int value = 0; value <= 255; ++value) {
		int addr = value;
		int valcpy = value;

		// Program ones, tenths, and hundreds
		for (int i = 0; i < 3; ++i) {
			int digit = digits[valcpy % 10];
			writeToMem(addr, digit);
			mem[addr] = digit;
			addr += 256;
			valcpy /= 10;
		}

		writeToMem(addr, 0); // Program sign
		mem[addr] = 0;
	}

	// Program signed int display
	for (int value = -128; value <= 127; ++value) {
		int addr = value + 1152;
		int valcpy = abs(value);

		// Program ones, tenths, and hundreds
		for (int i = 0; i < 3; ++i) {
			int digit = digits[valcpy % 10];
			writeToMem(addr, digit);
			mem[addr] = digit;
			addr += 256;
			valcpy /= 10;
		}

		// Program signs
		if (value < 0) {
			writeToMem(addr, 1);
			mem[addr] = 1;
		} else {
			writeToMem(addr, 0);
			mem[addr] = 0;
		}
	}

	// Error check
	for  (int addr = 0; addr <= MAX_ADDR; ++addr) {
		int val;
		if (val = readFromMem(addr) != mem[addr]) {
			return addr + 1;
		}
	}

	return 0;
}

// Setup eeprom for programming
void setUp() {
	wiringPiSetup();
	pinMode(OE, OUTPUT);
	pinMode(WE, OUTPUT);
	digitalWrite(WE, HIGH);
	sleep_for(nanoseconds(10));
}

int main() {
	setUp();
	clearMem(MAX_VAL);
	printf("Cleared\n");
	
	int code = programDisplay();
	
	if (code == 0) {
		printf("Programmed\n");
	} else {
		printf("Failed at address %d\n", code - 1);
	}
	
	return 0;
}

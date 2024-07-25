/*
  ShiftRegister74HC595.h - Library for simplified control of 74HC595 shift registers.
  Developed and maintained by Timo Denk and contributers, since Nov 2014.
  Additional information is available at https://timodenk.com/blog/shift-register-arduino-library/
  Released into the public domain.
*/

#pragma once

#include <Arduino.h>
#include <SPI.h>

template <uint8_t Size>
class ShiftRegisterTPIC6B595
{
public:
    ShiftRegisterTPIC6B595(const uint8_t serialDataPin, const uint8_t clockPin, const uint8_t latchPin);

    void setAll(const uint8_t *digitalValues);
#ifdef __AVR__
    void setAll_P(const uint8_t *digitalValuesProgmem); // Experimental, PROGMEM data
#endif
    uint8_t *getAll();
    void set(const uint8_t pin, const uint8_t value);
    void setNoUpdate(const uint8_t pin, uint8_t value);
    void updateRegisters();
    void setAllLow();
    void setAllHigh();
    uint8_t get(const uint8_t pin);

private:
    uint8_t _clockPin;
    uint8_t _serialDataPin;
    uint8_t _latchPin;

    uint8_t _digitalValues[Size];
};

#define SHIFTREG_DELAY_MICROSECONDS 5

// ShiftRegisterTPIC6B595 constructor
// Size is the number of shiftregisters stacked in serial
template <uint8_t Size>
ShiftRegisterTPIC6B595<Size>::ShiftRegisterTPIC6B595(const uint8_t serialDataPin, const uint8_t clockPin, const uint8_t latchPin)
{
    // set attributes
    _clockPin = clockPin;
    _serialDataPin = serialDataPin;
    _latchPin = latchPin;

    // define pins as outputs
    pinMode(clockPin, OUTPUT);
    pinMode(serialDataPin, OUTPUT);
    pinMode(latchPin, OUTPUT);

    // set pins low
    digitalWrite(clockPin, LOW);
    digitalWrite(serialDataPin, LOW);
    digitalWrite(latchPin, LOW);

    // allocates the specified number of bytes and initializes them to zero
    memset(_digitalValues, 0, Size * sizeof(uint8_t));

    updateRegisters(); // reset shift register
}

// Set all pins of the shift registers at once.
// digitalVAlues is a uint8_t array where the length is equal to the number of shift registers.
template <uint8_t Size>
void ShiftRegisterTPIC6B595<Size>::setAll(const uint8_t *digitalValues)
{
    memcpy(_digitalValues, digitalValues, Size); // dest, src, size
    updateRegisters();
}

// Experimental
// The same as setAll, but the data is located in PROGMEM
// For example with:
//     const uint8_t myFlashData[] PROGMEM = { 0x0F, 0x81 };
#ifdef __AVR__
template <uint8_t Size>
void ShiftRegisterTPIC6B595<Size>::setAll_P(const uint8_t *digitalValuesProgmem)
{
    PGM_VOID_P p = reinterpret_cast<PGM_VOID_P>(digitalValuesProgmem);
    memcpy_P(_digitalValues, p, Size);
    updateRegisters();
}
#endif

// Retrieve all states of the shift registers' output pins.
// The returned array's length is equal to the number of shift registers.
template <uint8_t Size>
uint8_t *ShiftRegisterTPIC6B595<Size>::getAll()
{
    return _digitalValues;
}

// Set a specific pin to either HIGH (1) or LOW (0).
// The pin parameter is a positive, zero-based integer, indicating which pin to set.
template <uint8_t Size>
void ShiftRegisterTPIC6B595<Size>::set(const uint8_t pin, const uint8_t value)
{
    setNoUpdate(pin, value);
    updateRegisters();
}

// Updates the shift register pins to the stored output values.
// This is the function that actually writes data into the shift registers of the TPIC6B595.
template <uint8_t Size>
void ShiftRegisterTPIC6B595<Size>::updateRegisters()
{
    uint8_t data;
    for (int i = Size - 1; i >= 0; i--)
    {
        data = _digitalValues[i];
        for (int p = 7; p >= 0; p--)
        {
            digitalWrite(_serialDataPin, (data >> p) & 1);
            delayMicroseconds(SHIFTREG_DELAY_MICROSECONDS);
            digitalWrite(_clockPin, HIGH);
            delayMicroseconds(SHIFTREG_DELAY_MICROSECONDS);
            digitalWrite(_clockPin, LOW);
            delayMicroseconds(SHIFTREG_DELAY_MICROSECONDS);
        }
    }
    digitalWrite(_serialDataPin, LOW);
    delayMicroseconds(SHIFTREG_DELAY_MICROSECONDS);
    digitalWrite(_latchPin, HIGH);
    delayMicroseconds(SHIFTREG_DELAY_MICROSECONDS);
    digitalWrite(_latchPin, LOW);
    delayMicroseconds(SHIFTREG_DELAY_MICROSECONDS);
}

// Equivalent to set(int pin, uint8_t value), except the physical shift register is not updated.
// Should be used in combination with updateRegisters().
template <uint8_t Size>
void ShiftRegisterTPIC6B595<Size>::setNoUpdate(const uint8_t pin, const uint8_t value)
{
    (value) ? bitSet(_digitalValues[pin / 8], pin % 8) : bitClear(_digitalValues[pin / 8], pin % 8);
}

// Returns the state of the given pin.
// Either HIGH (1) or LOW (0)
template <uint8_t Size>
uint8_t ShiftRegisterTPIC6B595<Size>::get(const uint8_t pin)
{
    return (_digitalValues[pin / 8] >> (pin % 8)) & 1;
}

// Sets all pins of all shift registers to HIGH (1).
template <uint8_t Size>
void ShiftRegisterTPIC6B595<Size>::setAllHigh()
{
    for (int i = 0; i < Size; i++)
    {
        _digitalValues[i] = 255;
    }
    updateRegisters();
}

// Sets all pins of all shift registers to LOW (0).
template <uint8_t Size>
void ShiftRegisterTPIC6B595<Size>::setAllLow()
{
    for (int i = 0; i < Size; i++)
    {
        _digitalValues[i] = 0;
    }
    updateRegisters();
}
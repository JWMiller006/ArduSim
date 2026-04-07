//
// Created by James Miller on 4/6/2026.
//

#pragma once
#include <cstdint>

#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 255
#define RUNNING arduino.running

struct DigitalPin
{
    uint8_t value;
    bool write_pin;
};

struct ArduinoMega
{
    DigitalPin pins[54];
    uint16_t analogInputs[16];
    bool running = true;
};

inline ArduinoMega arduino{};

void delay(unsigned int milliseconds);
void delayMicroseconds(unsigned int microseconds);

void pinMode(unsigned short pin, unsigned short mode);
void digitalWrite(unsigned int pin, unsigned int value);
unsigned int digitalRead(unsigned int pin);
void analogWrite(unsigned int pin, unsigned int value);
unsigned int analogRead(unsigned int pin);
long pulseIn(unsigned short pin, unsigned short pulse);
ArduinoMega getArduinoSnapshot();

class SerialClass
{
public:
    static void begin(unsigned long baud);
    static void print(const char* str);
    static void println(const char* str);
    static void print(float value);
    static void println(float value);
};

extern SerialClass Serial;

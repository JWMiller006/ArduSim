//
// Created by James Miller on 4/6/2026.
//

#include "Arduino.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
std::mutex arduinoMutex;
}

SerialClass Serial;

void delay(unsigned int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void delayMicroseconds(unsigned int microseconds)
{
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

void pinMode(unsigned short pin, unsigned short mode)
{
    std::lock_guard<std::mutex> lock(arduinoMutex);
    arduino.pins[pin].value = 0;
    arduino.pins[pin].write_pin = mode;
}

void digitalWrite(unsigned int pin, unsigned int value)
{
    std::lock_guard<std::mutex> lock(arduinoMutex);
    arduino.pins[pin].value = value;
}

unsigned int digitalRead(unsigned int pin)
{
    std::lock_guard<std::mutex> lock(arduinoMutex);
    return arduino.pins[pin].value;
}

void analogWrite(unsigned int pin, unsigned int value)
{
    std::lock_guard<std::mutex> lock(arduinoMutex);
    arduino.analogInputs[pin] = value;
}

unsigned int analogRead(unsigned int pin)
{
    std::lock_guard<std::mutex> lock(arduinoMutex);
    return arduino.analogInputs[pin];
}

ArduinoMega getArduinoSnapshot()
{
    std::lock_guard<std::mutex> lock(arduinoMutex);
    return arduino;
}

void setArduinoTheta(float theta)
{
    std::lock_guard<std::mutex> lock(arduinoMutex);
    arduino.theta = theta;
}

long pulseIn(unsigned short pin, unsigned short pulse)
{
    // For now, just return a dummy value
    return 1000;
}

void SerialClass::begin(unsigned long baud)
{
    // Do nothing for now
}

void SerialClass::print(const char* str)
{
    std::cout << str;
}

void SerialClass::println(const char* str)
{
    std::cout << str << std::endl;
}

void SerialClass::print(float value)
{
    std::cout << value;
}

void SerialClass::println(float value)
{
    std::cout << value << std::endl;
}


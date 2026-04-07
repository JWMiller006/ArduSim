//
// Created by James Miller on 4/6/2026.
//

#include "Enes100.h"
#include <iostream>
#define ENES100_STR "[Enes100]: "

float Enes100Class::x = 0.0f;
float Enes100Class::y = 0.0f;
float Enes100Class::theta = 0.0f;
const char* Enes100Class::mTeamName = nullptr;
unsigned short Enes100Class::mTeamType = 0;
unsigned short Enes100Class::mMarkerId = 0;
unsigned short Enes100Class::mRoomNumber = 0;
unsigned short Enes100Class::mWifiModuleTX = 0;
unsigned short Enes100Class::mWifiModuleRX = 0;

void Enes100Class::begin(const char* teamName, unsigned short teamType, int markerId, int roomNumber, int wifiModuleTX, int wifiModuleRX)
{
    mTeamName = teamName;
    mMarkerId = markerId;
    mRoomNumber = roomNumber;
    mWifiModuleTX = wifiModuleTX;
    mWifiModuleRX = wifiModuleRX;
    mTeamType = teamType;
}

void Enes100Class::print(const char* str)
{
    std::cout << ENES100_STR << str;
}

void Enes100Class::println(const char* str)
{
    std::cout << ENES100_STR << str << std::endl;
}

void Enes100Class::println()
{
    std::cout << std::endl;
}

void Enes100Class::print(float value)
{
    std::cout << ENES100_STR << value;
}

void Enes100Class::println(float value)
{
    std::cout << ENES100_STR << value << std::endl;
}

void Enes100Class::println(unsigned short value)
{
    std::cout << ENES100_STR << value << std::endl;
}

void Enes100Class::print(unsigned short value)
{
    std::cout << ENES100_STR << value;
}

float Enes100Class::getTheta()
{
    return theta;
}

float Enes100Class::getX()
{
    return x;
}

float Enes100Class::getY()
{
    return y;
}

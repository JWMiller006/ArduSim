//
// Created by James Miller on 4/6/2026.
//

#pragma once

struct ArduinoMega;

class Enes100Class
{
public:
    static float getTheta();
    static float getX();
    static float getY();
    static void setWheelDistanceCm(float distanceCm);
    static void println(const char* str);
    static void print(const char* str);
    static void println(float value);
    static void print(float value);
    static void println(unsigned short value);
    static void print(unsigned short value);
    static void println();
    static void begin(const char* teamName, unsigned short teamType, int markerId, int roomNumber, int wifiModuleTX, int wifiModuleRX);
    static void updatePoseEstimate();

private:
    static float getSignedWheelSpeedCmPerSec(const ArduinoMega& snapshot, unsigned int enablePin, unsigned int dirPin1, unsigned int dirPin2);

    static float x, y, theta;
    static const char* mTeamName;
    static unsigned short mTeamType;
    static unsigned short mMarkerId;
    static unsigned short mRoomNumber;
    static unsigned short mWifiModuleTX;
    static unsigned short mWifiModuleRX;
};

static Enes100Class Enes100;
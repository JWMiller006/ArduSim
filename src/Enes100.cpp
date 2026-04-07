//
// Created by James Miller on 4/6/2026.
//

#include "Enes100.h"
#include "Arduino.h"
#include "h_main/pin_defns.hpp"
#include "h_main/types.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#define ENES100_STR "[Enes100]: "
#define DBG false

namespace {
constexpr float kNoLoadPwm = 255.0f;
constexpr float kMaxSpeed = 19.2f;
constexpr float kMecanumRollerAngleDeg = 45.0f;
constexpr float kDefaultWheelDistanceCm = 33.0f;
constexpr float kSplitAxisAmount = 0.707106781187f; // 1/sqrt(2) to account for mecanum wheel roller angle

float gWheelDistanceCm = kDefaultWheelDistanceCm;

std::mutex gPoseMutex;
std::chrono::steady_clock::time_point gLastPoseUpdate = std::chrono::steady_clock::now();

float clampPwmToSpeedCmPerSec(const uint8_t pwm)
{
    const float ratio = std::clamp(static_cast<float>(pwm) / kNoLoadPwm, 0.0f, 1.0f);
    return ratio * kMaxSpeed;
}

float wrapRadians(float angle)
{
    constexpr float kPi = 3.14159265359f;
    constexpr float kTwoPi = 6.28318530718f;
    while (angle > kPi) {
        angle -= kTwoPi;
    }
    while (angle < -kPi) {
        angle += kTwoPi;
    }
    return angle;
}

float getRollerProjectionFactor()
{
    constexpr float kPi = 3.14159265359f;
    constexpr float angleRadians = kMecanumRollerAngleDeg * (kPi / 180.0f);
    return std::cos(angleRadians);
}
} // namespace

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
    std::lock_guard<std::mutex> lock(gPoseMutex);

    mTeamName = teamName;
    mMarkerId = markerId;
    mRoomNumber = roomNumber;
    mWifiModuleTX = wifiModuleTX;
    mWifiModuleRX = wifiModuleRX;
    mTeamType = teamType;

    x = 0.0f;
    y = 0.0f;
    theta = 0.0f;
    setArduinoTheta(theta);
    gLastPoseUpdate = std::chrono::steady_clock::now();
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
    updatePoseEstimate();
    return theta;
}

float Enes100Class::getX()
{
    updatePoseEstimate();
    return x;
}

float Enes100Class::getY()
{
    updatePoseEstimate();
    return y;
}

void Enes100Class::setWheelDistanceCm(float distanceCm)
{
    std::lock_guard<std::mutex> lock(gPoseMutex);
    if (distanceCm > 1.0f) {
        gWheelDistanceCm = distanceCm;
    }
}

float Enes100Class::getSignedWheelSpeedCmPerSec(const ArduinoMega& snapshot, unsigned int enablePin, unsigned int dirPin1, unsigned int dirPin2)
{
    if (!snapshot.pins[enablePin].write_pin) {
        std::cout << "Read nothing from the pin" << std::endl;
        return 0.0f;
    }

    const uint8_t d1 = snapshot.pins[dirPin1].value;
    const uint8_t d2 = snapshot.pins[dirPin2].value;

    int direction = 0;
    if (d1 == LOW && d2 == HIGH) {
        direction = 1;
    } else if (d1 == HIGH && d2 == LOW) {
        direction = -1;
    }

    if (direction == 0) {
        return 0.0f;
    }

    const float speedMagnitude = clampPwmToSpeedCmPerSec(snapshot.pins[enablePin].value);
    return speedMagnitude * static_cast<float>(direction);
}

void Enes100Class::updatePoseEstimate()
{
    std::lock_guard<std::mutex> lock(gPoseMutex);

    const auto now = std::chrono::steady_clock::now();
    const float dtSeconds = std::chrono::duration<float>(now - gLastPoseUpdate).count();
    gLastPoseUpdate = now;

    if (dtSeconds <= 0.0f) {
        return;
    }

    const ArduinoMega snapshot = getArduinoSnapshot();
    const float fr = getSignedWheelSpeedCmPerSec(snapshot, FORWARD_RIGHT_MOTOR_ENABLE, FORWARD_RIGHT_MOTOR_DRIVER_1, FORWARD_RIGHT_MOTOR_DRIVER_2);
    const float fl = getSignedWheelSpeedCmPerSec(snapshot, FORWARD_LEFT_MOTOR_ENABLE, FORWARD_LEFT_MOTOR_DRIVER_1, FORWARD_LEFT_MOTOR_DRIVER_2);
    const float rr = getSignedWheelSpeedCmPerSec(snapshot, REAR_RIGHT_MOTOR_ENABLE, REAR_RIGHT_MOTOR_DRIVER_1, REAR_RIGHT_MOTOR_DRIVER_2);
    const float rl = getSignedWheelSpeedCmPerSec(snapshot, REAR_LEFT_MOTOR_ENABLE, REAR_LEFT_MOTOR_DRIVER_1, REAR_LEFT_MOTOR_DRIVER_2);

    #if DBG
    printf("Wheel speeds (cm/s): FR: %+.2f, FL: %+.2f, RR: %+.2f, RL: %+.2f\n", fr, fl, rr, rl);
    #endif

    unsigned int forward_motors = 0;
    unsigned int rear_motors = 0;
    unsigned int off_motors = 0;

    std::string fs, rs, os;

    if (fr == 0.0f)
    {
        off_motors |= FrontRight;
        #if DBG
        os += "FrontRight ";
        #endif
    } else if (fr > 0.0f)
    {
        forward_motors |= FrontRight;
        #if DBG
        fs += "FrontRight ";
        #endif
    } else if (fr < 0.0f)
    {
        rear_motors |= FrontRight;
        #if DBG
        rs += "FrontRight ";
        #endif
    }

    if (rl == 0.0f)
    {
        off_motors |= RearLeft;
        #if DBG
        os += "RearLeft ";
        #endif
    } else if (rl < 0.0f)
    {
        rear_motors |= RearLeft;
        #if DBG
        rs += "RearLeft ";
        #endif
    } else if (rl > 0.0f)
    {
        forward_motors |= RearLeft;
        #if DBG
        fs += "RearLeft ";
        #endif
    }

    if (rr == 0.0f)
    {
        off_motors |= RearRight;
        #if DBG
        os += "RearRight ";
        #endif
    } else if (rr < 0.0f)
    {
        rear_motors |= RearRight;
        #if DBG
        rs += "RearRight ";
        #endif
    } else if (rr > 0.0f)
    {
        forward_motors |= RearRight;
        #if DBG
        fs += "RearRight ";
        #endif
    }

    if (fl == 0.0f)
    {
        off_motors |= FrontLeft;
        #if DBG
        os += "FrontLeft ";
        #endif
    } else if (fl < 0.0f)
    {
        rear_motors |= FrontLeft;
        #if DBG
        rs += "FrontLeft ";
        #endif
    } else if (fl > 0.0f)
    {
        forward_motors |= FrontLeft;
        #if DBG
        fs += "FrontLeft ";
        #endif
    }

    #if DBG
    std::cout << "Forward Motors: " << fs << std::endl;
    std::cout << "Rear Motors: " << rs << std::endl;
    std::cout << "Off Motors: " << os << std::endl;
    #endif

    unsigned int axis = 0;
    float omega_dir = 0.0f;
    float speed = 0.0f;
    #if DBG
    std::string debugString;
    #endif

    if ((forward_motors & (FrontLeft | RearRight | FrontRight | RearLeft)) == (FrontLeft | RearRight | FrontRight | RearLeft)){
        axis |= Forward | Drive; // Driving forward
        #if DBG
        debugString += "Drive Forward";
        #endif
    } else if ((forward_motors & (FrontLeft | RearRight)) == (FrontLeft | RearRight) && (off_motors & (FrontRight | RearLeft)) == (FrontRight | RearLeft)){
        axis |= Forward | Right | Strafe; // Strafing Forward-Right
        omega_dir = PI / 4.0f;
        #if DBG
        debugString += "Strafe Forward-Right";
        #endif
    } else if ((forward_motors & (FrontRight | RearLeft)) == (FrontRight | RearLeft) && (off_motors & (FrontLeft | RearRight)) == (FrontLeft | RearRight)){
        axis |= Forward | Left | Strafe; // Strafing Forward-Left
        omega_dir = 3.0f * PI / 4;
        #if DBG
        debugString += "Strafe Forward-Left";
        #endif
    } else if ((rear_motors & (FrontRight | RearLeft)) == (FrontRight | RearLeft) && (off_motors & (FrontLeft | RearRight)) == (FrontLeft | RearRight)){
        axis |= Backward | Right | Strafe; // Strafing Back-Right
        omega_dir = -1.0f * PI / 4.0f;
        #if DBG
        debugString += "Strafe Backward-Right";
        #endif
    } else if ((rear_motors & (FrontLeft | RearRight)) == (FrontLeft | RearRight) && (off_motors & (FrontRight | RearLeft)) == (FrontRight | RearLeft)){
        axis |= Backward | Left | Strafe; // Strafing Back-Left
        omega_dir = -3.0f * PI / 4.0f;
        #if DBG
        debugString += "Strafe Backward-Left";
        #endif
    } else if ((rear_motors & (FrontLeft | RearRight | FrontRight | RearLeft)) == (FrontLeft | RearRight | FrontRight | RearLeft)){
        axis |= Backward | Drive; // Drive backward
        #if DBG
        debugString += "Drive Backward";
        #endif
    } else if ((forward_motors & (FrontLeft | RearRight)) == (FrontLeft | RearRight) && (rear_motors & (FrontRight | RearLeft)) == (FrontRight | RearLeft)){
        axis |= Right | Strafe; // Strafe Right
        omega_dir = PI;
        #if DBG
        debugString += "Strafe Right";
        #endif
    } else if ((forward_motors & (FrontRight | RearLeft)) == (FrontRight | RearLeft) && (rear_motors & (FrontLeft | RearRight)) == (FrontLeft | RearRight)){
        axis |= Left | Strafe; // Strafe Left
        omega_dir = PI;
        #if DBG
        debugString += "Strafe Left";
        #endif
    } else if ((forward_motors & (FrontLeft | RearLeft)) == (FrontLeft | RearLeft) && (rear_motors & (FrontRight | RearRight)) == (FrontRight | RearRight)){
        axis |= Center | Turn | CW; // Turn clock-wise around the center
        #if DBG
        debugString += "Turn CW about Center";
        #endif
    } else if ((rear_motors & (FrontLeft | RearLeft)) == (FrontLeft | RearLeft) && (forward_motors & (FrontRight | RearRight)) == (FrontRight | RearRight)){
        axis |= (Center | Turn | CCW); // Turn counter-clock-wise around the center
        #if DBG
        debugString += "Turn CCW about Center";
        #endif
    } else if ((forward_motors & (FrontLeft)) && (rear_motors & FrontRight) && (off_motors & (RearRight | RearLeft)) == (RearRight | RearLeft)) {
        axis |= Backward | Turn | CW; // Turn about the back center clockwise
        #if DBG
        debugString += "Turn about back center CW";
        #endif
    } else if ((rear_motors & (FrontLeft)) && (forward_motors & FrontRight) && (off_motors & (RearRight | RearLeft)) == (RearRight | RearLeft)) {
        axis |= Backward | Turn | CCW; // Turn about the back center counter-clockwise
        #if DBG
        debugString += "Turn about back center CW";
        #endif
    } else if ((forward_motors & (RearRight)) && (rear_motors & RearLeft) && (off_motors & (FrontRight | FrontLeft)) == (FrontLeft | FrontRight)) {
        axis |= Forward | Turn | CW; // Turn about the forward center clockwise
        #if DBG
        debugString += "Turn about forward center CW";
        #endif
    } else if ((rear_motors & (RearRight)) && (forward_motors & RearLeft) && (off_motors & (FrontRight | FrontLeft)) == (FrontLeft | FrontRight)) {
        axis |= Forward | Turn | CCW; // Turn about the forward center counter-clockwise
        #if DBG
        debugString += "Turn about forward center CCW";
        #endif
    } else if ((forward_motors & RearRight) && (rear_motors & RearLeft) && (off_motors & (FrontRight | FrontLeft)) == (FrontRight | FrontLeft)) {
        axis |= Forward | Turn | CW; // Turn about the front center clockwise
        #if DBG
        debugString += "Turn about front center CW";
        #endif
    } else if ((rear_motors & RearRight) && (forward_motors & RearLeft) && (off_motors & (FrontRight | FrontLeft)) == (FrontRight | FrontLeft)) {
        axis |= Forward | Turn | CCW; // Turn about the front center counter-clockwise
        #if DBG
        debugString += "Turn about front center CW";
        #endif
    } else if ((forward_motors & (FrontLeft | RearLeft)) == (FrontLeft | RearLeft) && (off_motors & (FrontRight | RearRight)) == (FrontRight | RearRight)){
        axis |= Backward | Right | Turn; // turn about the back right wheel
        #if DBG
        debugString += "Turn about back right wheel";
        #endif
    } else if ((forward_motors & (FrontRight | RearRight)) == (FrontRight | RearRight) && (off_motors & (FrontLeft | RearLeft)) == (FrontLeft | RearLeft)){
        axis |= Backward | Left | Turn; // turn about the back left wheel
        #if DBG
        debugString += "Turn about back left wheel";
        #endif
    } else if ((rear_motors & (FrontLeft | RearLeft)) == (FrontLeft | RearLeft) && (off_motors & (FrontRight | RearRight)) == (FrontRight | RearRight)){
        axis |= Forward | Right | Turn; // turn about the back right wheel
        #if DBG
        debugString += "Turn about back right wheel";
        #endif
    } else if ((rear_motors & (FrontRight | RearRight)) == (FrontRight | RearRight) && (off_motors & (FrontLeft | RearLeft)) == (FrontLeft | RearLeft)){
        axis |= Forward | Left | Turn; // turn about the back left wheel
        #if DBG
        debugString += "Turn about back left wheel";
        #endif
    } else {
        axis |= Center;
        #if DBG
        debugString += "Do squat";
        #endif
    }

    float vx = 0.0f, vy = 0.0f;

    speed = fr * kSplitAxisAmount;

    if ((axis & (Turn | CW)) == (Turn | CW)){
        omega_dir = -1.0f;
    } else if ((axis & (Turn | CCW)) == (Turn | CCW)){
        omega_dir = 1.0f;
    } else if (axis & Drive) {
        speed = fr;
        vx = cosf(theta) * speed;
        vy = sinf(theta) * speed;
    } else if (axis & Strafe) {
        speed = fr * kSplitAxisAmount;

        vx = cosf(theta + omega_dir) * speed;
        vy = sinf(theta + omega_dir) * speed;

        omega_dir = 0.0f;
    }

    x += vx * dtSeconds;
    y += vy * dtSeconds;
    theta = wrapRadians(theta + omega_dir * speed * kSplitAxisAmount * dtSeconds * 0.5f);
    setArduinoTheta(theta);

    #if DBG
    std::cout << debugString << " | vx: " << vx << " cm/s, vy: " << vy << " cm/s, omega_dir: " << omega_dir << " rad/s, theta: " << theta << " rad" << std::endl;
    #endif
}


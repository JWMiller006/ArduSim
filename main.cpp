#include <thread>
#include <chrono>
#include "Arduino.h"
#include "h_main/h_main.ino"
#include "ui/window_manager.hpp"

int main() {
    ui::startWindowManager();

    std::thread simulationThread([]() {
        setup();

        while (!ui::isWindowManagerStopRequested())
        {
            loop();
        }
    });
    simulationThread.detach();

    while (!ui::isWindowManagerStopRequested())
    {

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ui::requestWindowManagerStop();
    ui::joinWindowManager();

    return 0;
}

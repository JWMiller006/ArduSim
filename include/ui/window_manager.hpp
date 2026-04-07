#pragma once

namespace ui {

struct WindowManagerConfig {
    const char* title = "mega_sim pin monitor";
    int width = 1280;
    int height = 720;
};

bool startWindowManager(const WindowManagerConfig& config = {});
void requestWindowManagerStop();
bool isWindowManagerStopRequested();
void joinWindowManager();

} // namespace ui


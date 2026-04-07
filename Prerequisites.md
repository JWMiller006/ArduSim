# Installation Prerequisites
Before you can install the software, you need to ensure that your system meets the following prerequisites:
1. **Operating System**: This software has been tested on Windows 11 and Ubuntu 24.04 and should 
    theoretically work the same on Windows 10 and Ubuntu 22.04, but these have not been tested. The code
    in theory should also work on MacOS, but this has not been tested and is not guaranteed to work.
2. **CMake**: You need to have CMake installed on your system. You can download it from the official website: https://cmake.org/download/
3. **C++ Compiler**: You need to have a C++ compiler installed on your system. For Windows, you can use Visual Studio or MinGW. For Linux, you can use GCC or Clang. For MacOS, you can use Xcode.
4. **GLFW**: You need to have the GLFW library installed on your system. You can download it from the official website: https://www.glfw.org/download.html
5. **Vulkan SDK**: You need to have the Vulkan SDK installed on your system. You can download it from the official website: https://vulkan.lunarg.com/sdk/home

## Installation Steps
### Windows 10/11
1. ***(Suggestion)*** Download and install Visual Studio from the official website (suggestion to use the Community edition).
    - You don't have to use Visual Studio, but you need to have a C++ compiler installed on your system, and Visual Studio is the easiest way to get one on Windows. If you choose to use Visual Studio, make sure to select the "Desktop development with C++" workload during installation.
2. Download and install CMake from the official website (suggestion to use the msi installer).
3. Download and install the Vulkan SDK from the official website. Make sure to follow the installation instructions provided on the website.
    - During installation, make sure to select the option to add the Vulkan SDK to your system PATH environment variable.
4. Download and install the GLFW library from the official website. Make sure to follow the installation instructions provided on the website.
    - During installation, make sure to select the option to add the GLFW library to your system PATH environment variable.
    - Alternatively, you can build the GLFW library from source using CMake. To do this, follow the instructions provided in the GLFW documentation: https://www.glfw.org/docs/latest/compile.html

### Linux (Ubuntu 22.04/24.04)
1. Open a terminal and update your package manager:
    ```bash
    sudo apt update
    ```
2. Install the required packages:
    ```bash
    sudo apt install build-essential cmake libglfw3-dev vulkan-sdk
    ```
3. If the Vulkan SDK is not available in your package manager, you can download and install it from the official website. Make sure to follow the installation instructions provided on the website.
    - During installation, make sure to add the Vulkan SDK to your system PATH environment variable.
4. If the GLFW library is not available in your package manager, you can download and install it from the official website. Make sure to follow the installation instructions provided on the website.

### MacOS
I have no idea how MacOS works and thus cannot provide detailed instructions for installing the prerequisites on MacOS. 
    That said, you still need to have CMake, a C++ compiler, the Vulkan SDK, and the GLFW library installed on your system. You can download and install these from their respective official websites. 
    Make sure to follow the installation instructions provided on the websites and add the necessary paths to your system PATH environment variable.

## Environment Suggestions
- It is recommended to use an Integrated Development Environment (IDE) such as Visual Studio, CLion, or Visual Studio Code for developing and building the software. 
    - I personally use CLion since it is easiest to set up with CMake, but you can use any IDE that supports C++ development and CMake.
- Make sure to configure your IDE to use the correct C++ compiler and to include the necessary paths for the Vulkan SDK and GLFW library. 
    - In Visual Studio, you can do this by going to the project properties and setting the include directories and library directories under the "VC++ Directories" section.
    - In CLion, you can do this by modifying the CMakeLists.txt file to include the necessary paths for the Vulkan SDK and GLFW library. This should already be set up to find these automatically.
    - In Visual Studio Code, you can do this by modifying the c_cpp_properties.json file to include the necessary paths for the Vulkan SDK and GLFW library.
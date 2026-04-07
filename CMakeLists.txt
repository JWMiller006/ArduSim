cmake_minimum_required(VERSION 4.2)
project(mega_sim)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MEGA_SIM_ENABLE_IMGUI_UI "Enable ImGui GLFW + Vulkan pin monitor UI" ON)
option(MEGA_SIM_BUNDLE_GLFW "Copy GLFW runtime next to mega_sim executable on Windows" ON)
option(MEGA_SIM_BUNDLE_MINGW_RUNTIME "Copy MinGW runtime DLLs next to mega_sim executable on Windows" ON)

# Get Directories
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}/Documents" DOCUMENTS_DIR)
elseif(UNIX)
    set(DOCUMENTS_DIR "$ENV{HOME}/Documents")
elseif(APPLE)
    set(DOCUMENTS_DIR "$ENV{HOME}/Documents")
    message(STATUS "User Documents folder: ${USER_DOCUMENTS_DIR}")
endif()

if(EXISTS "${DOCUMENTS_DIR}")
    message(STATUS "Found Documents at: ${DOCUMENTS_DIR}")
else()
    message(WARNING "Documents directory not found at: ${DOCUMENTS_DIR}")
endif()


# Add source files
file(GLOB PROJ_SRCS
    "proj_dir/h_main/*.cpp"
    "proj_dir/h_main/*.h"
    "proj_dir/h_main/*.ino"
)

message(STATUS "Found source files:")
foreach(src ${PROJ_SRCS})
    message(STATUS "  ${src}")
endforeach()

set(SOURCE
    main.cpp
    include/Arduino.h
    src/Arduino.cpp
    include/Enes100.h
    src/Enes100.cpp
    include/ui/window_manager.hpp
    src/ui/window_manager.cpp
)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
set(IMGUI_SOURCES)
set(MEGA_SIM_UI_DEPENDENCIES_READY OFF)

if(MEGA_SIM_ENABLE_IMGUI_UI AND EXISTS "${IMGUI_DIR}/imgui.h")
    find_package(Vulkan QUIET)
    find_package(glfw3 QUIET)

    if(Vulkan_FOUND AND glfw3_FOUND)
        set(IMGUI_SOURCES
            ${IMGUI_DIR}/imgui.cpp
            ${IMGUI_DIR}/imgui_draw.cpp
            ${IMGUI_DIR}/imgui_tables.cpp
            ${IMGUI_DIR}/imgui_widgets.cpp
            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        )
        set(MEGA_SIM_UI_DEPENDENCIES_READY ON)
    else()
        message(WARNING "ImGui UI disabled: Vulkan and/or glfw3 not found.")
    endif()
elseif(MEGA_SIM_ENABLE_IMGUI_UI)
    message(WARNING "ImGui UI disabled: third_party/imgui not found.")
endif()

add_executable(mega_sim ${SOURCE} ${PROJ_SRCS} ${IMGUI_SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(mega_sim PRIVATE Threads::Threads)

if(WIN32 AND MINGW AND MEGA_SIM_BUNDLE_MINGW_RUNTIME)
    get_filename_component(MEGA_SIM_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MEGA_SIM_MINGW_RUNTIME_DLLS
        libwinpthread-1.dll
        libstdc++-6.dll
        libgcc_s_seh-1.dll
        libgcc_s_dw2-1.dll
        libgcc_s_sjlj-1.dll
    )

    set(MEGA_SIM_FOUND_MINGW_RUNTIME_DLLS 0)
    foreach(dll_name IN LISTS MEGA_SIM_MINGW_RUNTIME_DLLS)
        if(EXISTS "${MEGA_SIM_MINGW_BIN_DIR}/${dll_name}")
            math(EXPR MEGA_SIM_FOUND_MINGW_RUNTIME_DLLS "${MEGA_SIM_FOUND_MINGW_RUNTIME_DLLS} + 1")
            add_custom_command(TARGET mega_sim POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MEGA_SIM_MINGW_BIN_DIR}/${dll_name}"
                        $<TARGET_FILE_DIR:mega_sim>
                VERBATIM
            )
        endif()
    endforeach()

    if(MEGA_SIM_FOUND_MINGW_RUNTIME_DLLS EQUAL 0)
        message(WARNING "MEGA_SIM_BUNDLE_MINGW_RUNTIME is ON, but no MinGW runtime DLLs were found in ${MEGA_SIM_MINGW_BIN_DIR}.")
    endif()
endif()

if(MEGA_SIM_UI_DEPENDENCIES_READY)
    target_compile_definitions(mega_sim PRIVATE MEGA_SIM_HAS_IMGUI_UI=1)
    target_link_libraries(mega_sim PRIVATE glfw Vulkan::Vulkan)

    if(WIN32 AND MEGA_SIM_BUNDLE_GLFW)
        add_custom_command(TARGET mega_sim POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:glfw>
                    $<TARGET_FILE_DIR:mega_sim>
            VERBATIM
        )
    endif()
endif()

include_directories(
    include/
    proj_dir/
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
#    ${DOCUMENTS_DIR}/Arduino/libraries/ENES100/src
)
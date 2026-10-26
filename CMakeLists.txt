# CMake required version
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK
include(pico_sdk_import.cmake)

project(SmartRemocon C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/FreeRTOS-Kernel")
include(FreeRTOS_Kernel_import.cmake)

# --- Add subdirectories for each layer ---
add_subdirectory(src/utils)
add_subdirectory(src/wrappers)
add_subdirectory(src/modules)
add_subdirectory(src/apps)

# --- MQTT broker address ---
set(MQTT_BROKER_IP "192.168.0.10" CACHE STRING "MQTT broker IP address")
target_compile_definitions(modules PUBLIC MQTT_BROKER_IP="${MQTT_BROKER_IP}")

# --- Main Executable ---
add_executable(SmartRemocon
    src/main.c
)

pico_set_program_name(SmartRemocon "SmartRemocon")
pico_set_program_version(SmartRemocon "0.1")

pico_enable_stdio_uart(SmartRemocon 0)
pico_enable_stdio_usb(SmartRemocon 1)

# Include main source directory
target_include_directories(SmartRemocon PRIVATE src)

# Link layers and standard libraries to the main executable
target_link_libraries(SmartRemocon
    PRIVATE
    apps
    modules
    utils
    pico_stdlib
)

pico_add_extra_outputs(SmartRemocon)

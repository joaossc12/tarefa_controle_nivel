# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(tareja_controle_nivel C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(tareja_controle_nivel tareja_controle_nivel.c 
                                    lib/rele.c 
                                    lib/ssd1306.c
                                    lib/sensor.c
                                    lib/ws2812b.c)

pico_set_program_name(tareja_controle_nivel "tareja_controle_nivel")
pico_set_program_version(tareja_controle_nivel "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(tareja_controle_nivel 1)
pico_enable_stdio_usb(tareja_controle_nivel 1)

# Add the standard library to the build
target_link_libraries(tareja_controle_nivel
        pico_stdlib
        hardware_adc
        hardware_i2c
        hardware_pio
        hardware_pwm
        pico_stdlib     
        pico_cyw43_arch_lwip_threadsafe_background
        )

# Add the standard include files to the build
target_include_directories(tareja_controle_nivel PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/lib
)

# Add any user requested libraries
target_link_libraries(tareja_controle_nivel 
        
        )

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812b.pio)

pico_add_extra_outputs(tareja_controle_nivel)


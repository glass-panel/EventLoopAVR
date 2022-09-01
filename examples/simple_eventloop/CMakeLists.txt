cmake_minimum_required(VERSION 3.11)
# Project name
project("simple_eventloop")

# Product filename
set(PRODUCT_NAME "simple_eventloop")
# And there is no need for us to install a avr binary in our PC!
set(CMAKE_SKIP_INSTALL_RULES True)

# CPU, you can find the list here:
# https://gcc.gnu.org/onlinedocs/gcc/AVR-Options.html
set(MCU atmega328p)

# Use AVR GCC toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CXX_COMPILER avr-g++)
set(CMAKE_C_COMPILER avr-gcc)
set(CMAKE_ASM_COMPILER avr-gcc)

file(GLOB SRC_FILES "./*.cpp" "./*.c") # Load all files in src folder
include_directories("${PRODUCT_DIR}/../../include/" "$ENV{HOME}/Dev/avr/libraries/avr/include/")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# mmcu MUST be passed to bot the compiler and linker, this handle the linker
set(CMAKE_EXE_LINKER_FLAGS -mmcu=${MCU})
link_directories("$ENV{HOME}/Dev/avr/firmwares/")

add_compile_options(
    -mmcu=${MCU} # MCU
    -std=c++11
    -Wall # enable warnings
    -Wno-main
    -Wundef
    -pedantic
    -Wfatal-errors
    -fno-threadsafe-statics # need this for singleton's static self
    -fno-exceptions # no need for avr
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g)
    add_definitions("-DDEBUG")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options("-O3" "-Wl,--relax,--gc-sections")  # performace optimize and remove unreferenced code
    add_definitions("-DRELEASE")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    add_compile_options("-Os" "-Wl,--relax,--gc-sections")  # code size optimize and remove unreferenced code
    add_definitions("-DRELEASE")
    add_definitions("-DMINSIZE")
endif()

# Create one target
add_executable(${PRODUCT_NAME} ${SRC_FILES})

# Rename the output to .elf as we will create multiple files
set_target_properties(${PRODUCT_NAME} PROPERTIES OUTPUT_NAME "./${PRODUCT_NAME}.elf")
add_custom_target("hex" ALL 
                  avr-objcopy -j .text -j .data -O ihex "./${PRODUCT_NAME}.elf" "./${PRODUCT_NAME}.hex" 
                  DEPENDS ${PRODUCT_NAME})

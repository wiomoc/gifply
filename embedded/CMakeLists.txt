cmake_minimum_required(VERSION 3.13)
# Pull in SDK (must be before project)
set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
set(PICO_SDK_INIT_CMAKE_FILE ${PICO_SDK_PATH}/pico_sdk_init.cmake)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(gifply C CXX ASM)
pico_sdk_init()

add_executable(gifply
        gifply.c
        usb_descriptors.c
        gifdec.c
        )
target_include_directories(gifply PRIVATE ${CMAKE_CURRENT_LIST_DIR})
# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(gifply pico_stdlib pico_multicore hardware_pio hardware_flash tinyusb_device)

pico_generate_pio_header(gifply ${CMAKE_CURRENT_LIST_DIR}/ili9341_lcd.pio)
# create map/bin/hex file etc.
pico_add_extra_outputs(gifply)


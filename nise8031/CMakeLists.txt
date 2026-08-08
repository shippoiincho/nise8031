add_executable(nise8031)

target_include_directories(nise8031 PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_sources(nise8031 PRIVATE nise8031.c Z80.c fdc.c lcd.c)

pico_enable_stdio_usb(nise8031 1)
pico_enable_stdio_uart(nise8031 0)

add_subdirectory(../FatFS/src build)

target_compile_definitions(nise8031 PRIVATE Z80_STATIC Z80_WITH_LOCAL_HEADER Z80_WITH_Q Z80_WITH_ZILOG_NMOS_LD_A_IR_BUG Z80_WITH_FULL_IM0)
target_link_libraries(nise8031 PRIVATE pico_stdlib hardware_timer hardware_dma hardware_uart hardware_flash pico_multicore hardware_pwm hardware_i2c no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
pico_add_extra_outputs(nise8031)

######################################
# building variables
######################################
# debug build?
set(DEBUG 1)
# optimization
set(OPT -Og)

set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/lib/pico-sdk)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(kaluma-project C CXX ASM)
# initialize the Pico SDK
pico_sdk_init()

set(TARGET_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(TARGET_INC_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

set(SOURCES
  ${SOURCES}
  ${TARGET_SRC_DIR}/adc.c
  ${TARGET_SRC_DIR}/ringbuffer.c
  ${TARGET_SRC_DIR}/system.c
  ${TARGET_SRC_DIR}/gpio.c
  ${TARGET_SRC_DIR}/pwm.c
  ${TARGET_SRC_DIR}/tty.c
  ${TARGET_SRC_DIR}/flash.c
  ${TARGET_SRC_DIR}/storage.c
  ${TARGET_SRC_DIR}/uart.c
  ${TARGET_SRC_DIR}/i2c.c
  ${TARGET_SRC_DIR}/spi.c)

include_directories(${TARGET_INC_DIR} )

set(TARGET_HEAPSIZE 96)
set(JERRY_TOOLCHAIN toolchain_mcu_cortexm0plus.cmake)

set(KALUMA_MODULES events gpio led button pwm adc i2c spi uart graphics at storage stream http url startup)

set(KALUMA_MODULE_PWM 1)
set(KALUMA_MODULE_I2C 1)
set(KALUMA_MODULE_SPI 1)
set(KALUMA_MODULE_STORAGE 1)
set(KALUMA_MODULE_UART 1)
set(KALUMA_MODULE_GRAPHICS 1)

set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus)
set(CMAKE_C_FLAGS "-march=armv6-m -mcpu=cortex-m0plus -mthumb ${OPT} -Wall -fdata-sections -ffunction-sections")
if(DEBUG EQUAL 1)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -gdwarf-2")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -MMD -MP")

set(PREFIX arm-none-eabi-)
set(CMAKE_ASM_COMPILER ${PREFIX}gcc)
set(CMAKE_C_COMPILER ${PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${PREFIX}g++)
set(CMAKE_LINKER ${PREFIX}ld)
set(CMAKE_OBJCOPY ${PREFIX}objcopy)

set(TARGET_LIBS c nosys m pico_stdlib hardware_adc hardware_pwm hardware_i2c hardware_spi hardware_uart)
set(CMAKE_EXE_LINKER_FLAGS "-specs=nano.specs -u _printf_float -Wl,-Map=${TARGET}.map,--cref,--gc-sections")
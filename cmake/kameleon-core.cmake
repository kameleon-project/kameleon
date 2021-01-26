project(kameleon C ASM)

######################################
# building variables
######################################
# debug build?
set(DEBUG 1)
# optimization
set(OPT -Og)
# with bootloader
set(BOOTLOADER 1)

include(../targets/boards/kameleon-core/target.cmake)

set(JERRY_ROOT ${CMAKE_SOURCE_DIR}/../deps/jerryscript)
set(JERRY_INC
  ${JERRY_ROOT}/jerry-core
  ${JERRY_ROOT}/jerry-core/api
  ${JERRY_ROOT}/jerry-core/debugger
  ${JERRY_ROOT}/jerry-core/ecma/base
  ${JERRY_ROOT}/jerry-core/ecma/builtin-objects
  ${JERRY_ROOT}/jerry-core/ecma/builtin-objects/typedarray
  ${JERRY_ROOT}/jerry-core/ecma/operations
  ${JERRY_ROOT}/jerry-core/include
  ${JERRY_ROOT}/jerry-core/jcontext
  ${JERRY_ROOT}/jerry-core/jmem
  ${JERRY_ROOT}/jerry-core/jrt
  ${JERRY_ROOT}/jerry-core/lit
  ${JERRY_ROOT}/jerry-core/parser/js
  ${JERRY_ROOT}/jerry-core/parser/regexp
  ${JERRY_ROOT}/jerry-core/vm
  ${JERRY_ROOT}/jerry-ext/arg
  ${JERRY_ROOT}/jerry-ext/include
  ${JERRY_ROOT}/jerry-libm)

set(JERRY_ARGS
  --toolchain=cmake/toolchain_mcu_stm32f4.cmake
  --lto=OFF
  --error-messages=ON
  --js-parser=ON
  --mem-heap=${TARGET_HEAPSIZE}
  --mem-stats=ON
  --snapshot-exec=ON
  --line-info=ON
  --vm-exec-stop=ON
  --profile=es2015-subset
  --jerry-cmdline=OFF
  --cpointer-32bit=ON)

set(SRC_DIR ../src)
set(KAMELEON_GENERATED_C
  ${SRC_DIR}/gen/kameleon_modules.c
  ${SRC_DIR}/gen/kameleon_magic_strings.c)
set(KAMELEON_GENERATED_H
  ${SRC_DIR}/gen/kameleon_modules.h
  ${SRC_DIR}/gen/kameleon_magic_strings.h)

set(KAMELEON_GENERATED ${KAMELEON_GENERATED_C} ${KAMELEON_GENERATED_H})

string (REPLACE ";" " " KAMELEON_MODULE_LIST "${KAMELEON_MODULES}")

set(JERRY_LIBS
  ${JERRY_ROOT}/build/lib/libjerry-core.a
  ${JERRY_ROOT}/build/lib/libjerry-ext.a)

add_custom_command(OUTPUT ${JERRY_LIBS}
  DEPENDS ${KAMELEON_GENERATED_C}
  WORKING_DIRECTORY ${JERRY_ROOT}
  COMMAND python tools/build.py --clean ${JERRY_ARGS})

add_custom_command(OUTPUT ${KAMELEON_GENERATED_C}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/..
  COMMAND python ${JERRY_ROOT}/tools/build.py --clean --jerry-cmdline-snapshot=ON --snapshot-save=ON --snapshot-exec=ON --profile=es2015-subset
  COMMAND node tools/js2c.js --modules=${KAMELEON_MODULE_LIST} --target=${TARGET}
  COMMAND rm -rf deps/jerryscript/build)

#=============================================================
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)
set(CMAKE_C_FLAGS "-mcpu=cortex-m4 -mlittle-endian -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard ${OPT} -Wall -fdata-sections -ffunction-sections")
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


set(KAMELEON_INC ../include ../include/port ${SRC_DIR}/gen ${SRC_DIR}/modules)
include_directories(${KAMELEON_INC} ${JERRY_INC})

list(APPEND SOURCES
  ${SRC_DIR}/main.c
  ${SRC_DIR}/utils.c
  ${SRC_DIR}/base64.c
  ${SRC_DIR}/io.c
  ${SRC_DIR}/runtime.c
  ${SRC_DIR}/repl.c
  ${SRC_DIR}/jerry_port.c
  ${SRC_DIR}/jerryxx.c
  ${SRC_DIR}/global.c
  ${SRC_DIR}/ymodem.c
  ${KAMELEON_GENERATED_C})

if(KAMELEON_MODULE_PWM)
  list(APPEND SOURCES ${SRC_DIR}/modules/pwm/module_pwm.c)
  include_directories(${SRC_DIR}/modules/pwm)
endif()
if(KAMELEON_MODULE_I2C)
  list(APPEND SOURCES ${SRC_DIR}/modules/i2c/module_i2c.c)
  include_directories(${SRC_DIR}/modules/i2c)
endif()
if(KAMELEON_MODULE_SPI)
  list(APPEND SOURCES ${SRC_DIR}/modules/spi/module_spi.c)
  include_directories(${SRC_DIR}/modules/spi)
endif()
if(KAMELEON_MODULE_STORAGE)
  list(APPEND SOURCES ${SRC_DIR}/modules/storage/module_storage.c)
  include_directories(${SRC_DIR}/modules/storage)  
endif()
if(KAMELEON_MODULE_UART)
  list(APPEND SOURCES ${SRC_DIR}/modules/uart/module_uart.c)
  include_directories(${SRC_DIR}/modules/uart)  
endif()
if(KAMELEON_MODULE_GRAPHICS)
  list(APPEND SOURCES
    ${SRC_DIR}/modules/graphics/gc_cb_prims.c
    ${SRC_DIR}/modules/graphics/gc_1bit_prims.c
    ${SRC_DIR}/modules/graphics/gc_16bit_prims.c
    ${SRC_DIR}/modules/graphics/gc.c
    ${SRC_DIR}/modules/graphics/font_default.c
    ${SRC_DIR}/modules/graphics/module_graphics.c)
  include_directories(${SRC_DIR}/modules/graphics)
endif()

add_executable(kameleon-core.elf ${SOURCES} ${JERRY_LIBS})
target_link_libraries(kameleon-core.elf ${JERRY_LIBS} c nosys m)

add_custom_command(OUTPUT kameleon-core.hex kameleon-core.bin
  COMMAND ${CMAKE_OBJCOPY} -O ihex kameleon-core.elf kameleon-core.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary -S kameleon-core.elf kameleon-core.bin
  DEPENDS kameleon-core.elf)

add_custom_target(kameleon ALL DEPENDS kameleon-core.hex kameleon-core.bin)

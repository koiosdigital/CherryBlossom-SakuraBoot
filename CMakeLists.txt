cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME sakura_boot)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: ${CMAKE_BUILD_TYPE}")

enable_language(C ASM)

# Version: dev/local = v0.0.1, CI tag vX.Y.Z = 0x00XXYYZZ
set(SAKURA_BOOT_VERSION 0x00000001)
if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS})
    execute_process(
        COMMAND git describe --tags --exact-match --match "v[0-9]*.[0-9]*.[0-9]*"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_TAG_RESULT
    )
    if(GIT_TAG_RESULT EQUAL 0 AND GIT_TAG MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        set(VERSION_MAJOR ${CMAKE_MATCH_1})
        set(VERSION_MINOR ${CMAKE_MATCH_2})
        set(VERSION_PATCH ${CMAKE_MATCH_3})
        math(EXPR SAKURA_BOOT_VERSION
            "(${VERSION_MAJOR} << 16) | (${VERSION_MINOR} << 8) | ${VERSION_PATCH}"
            OUTPUT_FORMAT HEXADECIMAL)
        message("Bootloader version: v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} (${SAKURA_BOOT_VERSION})")
    else()
        message("No valid git tag found, using dev version: v0.0.1 (${SAKURA_BOOT_VERSION})")
    endif()
else()
    message("Dev build (not CI), bootloader version: v0.0.1 (${SAKURA_BOOT_VERSION})")
endif()

# Sources
set(SOURCES
    startup_stm32f103xb.s
    Core/Src/system_stm32f1xx.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
    can/fcm32_can.c
    can/can_hw.c
    src/main.c
    src/flash.c
    src/can_boot.c
    src/crc16.c
    src/fasthash.c
    src/led.c
)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc
    Drivers/STM32F1xx_HAL_Driver/Inc
    Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F1xx/Include
    Drivers/CMSIS/Include
    can
    src
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32F103xB
    SAKURA_BOOT_VERSION=${SAKURA_BOOT_VERSION}
    $<$<CONFIG:Debug>:DEBUG>
)

# Post-build: generate .bin and .hex
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Generating .bin and .hex, checking size"
)

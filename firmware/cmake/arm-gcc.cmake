# ARM GCC cross-compilation toolchain file for STM32F401RE (NUCLEO)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

# Prefer STM32CubeIDE toolchain (includes newlib-nano) over bare Homebrew GCC
find_program(_CUBE_GCC arm-none-eabi-gcc
    PATHS /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.macos64_1.0.100.202509120712/tools/bin
    NO_DEFAULT_PATH
)
if(_CUBE_GCC)
    get_filename_component(_TC_BIN "${_CUBE_GCC}" DIRECTORY)
else()
    set(_TC_BIN "")  # Fall back to PATH
endif()

set(CMAKE_C_COMPILER   ${_TC_BIN}/arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER ${_TC_BIN}/arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      ${_TC_BIN}/arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP      ${_TC_BIN}/arm-none-eabi-objdump)
set(CMAKE_SIZE         ${_TC_BIN}/arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

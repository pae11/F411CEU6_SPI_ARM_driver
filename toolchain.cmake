# Update your toolchain.cmake to use this path:
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(CMAKE_C_COMPILER /Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER /Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi/bin/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER /Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi/bin/arm-none-eabi-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

#############################################################################################################################
# file:  CMakeLists.txt
# brief: Template "CMakeLists.txt" for building of executables and static libraries with CMSIS Driver support.
#
# usage: Edit "VARIABLES"-section to suit project requirements.
#        For debug build:
#          cmake -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake  -S ./ -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
#          ninja -C build
#        For release build:
#          cmake -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake  -S ./ -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#          ninja -C build
#############################################################################################################################
cmake_minimum_required(VERSION 3.20)

# project() должен быть объявлен ДО FetchContent чтобы toolchain был активен
project(F411CEU6_SPI_ARM_driver)
enable_language(C ASM)

###################### CONSTANTS ######################################
# Определяем типы проектов
set(PROJECT_TYPE_EXECUTABLE          "exe")
set(PROJECT_TYPE_STATIC_LIBRARY      "static-lib")

# Определяем параметры процессоров
set(MCPU_CORTEX_M0                  "-mcpu=cortex-m0")
set(MCPU_CORTEX_M0PLUS              "-mcpu=cortex-m0plus")
set(MCPU_CORTEX_M3                  "-mcpu=cortex-m3")
set(MCPU_CORTEX_M4                  "-mcpu=cortex-m4")
set(MCPU_CORTEX_M7                  "-mcpu=cortex-m7")
set(MCPU_CORTEX_M33                 "-mcpu=cortex-m33")
set(MCPU_CORTEX_M55                 "-mcpu=cortex-m55")
set(MCPU_CORTEX_M85                 "-mcpu=cortex-m85")

# Параметры FPU
set(MFPU_FPV4_SP_D16               "-mfpu=fpv4-sp-d16")
set(MFPU_FPV5_D16                  "-mfpu=fpv5-d16")

# Параметры библиотек времени выполнения
set(RUNTIME_LIBRARY_REDUCED_C        "--specs=nano.specs")
set(RUNTIME_LIBRARY_STD_C            "")
set(RUNTIME_LIBRARY_SYSCALLS_MINIMAL "--specs=nosys.specs")
set(RUNTIME_LIBRARY_SYSCALLS_NONE    "")

# Параметры FPU ABI
set(MFLOAT_ABI_SOFTWARE             "-mfloat-abi=soft")
set(MFLOAT_ABI_HARDWARE             "-mfloat-abi=hard")
set(MFLOAT_ABI_MIX                  "-mfloat-abi=softfp")









###################### AUTOMATIC DOWNLOAD ######################################
include(FetchContent)

# Отключить загрузку: cmake -DAUTO_FETCH=OFF ...
option(AUTO_FETCH "Automatically download CMSIS packages via FetchContent" ON)

if(AUTO_FETCH)
    message(STATUS "=== AUTO_FETCH: скачиваем CMSIS пакеты ===")

    # ------------------------------------------------------------------
    # 1. CMSIS_5 — Core заголовки (core_cm4.h, cmsis_gcc.h ...)
    #             + Driver headers (Driver_GPIO.h, Driver_SPI.h ...)
    # ------------------------------------------------------------------
    FetchContent_Declare(
        cmsis5
        GIT_REPOSITORY https://github.com/ARM-software/CMSIS_5.git
        GIT_TAG        5.9.0
        GIT_SHALLOW    ON
    )

    # ------------------------------------------------------------------
    # 2. cmsis_device_f4 — ST официальный Device pack для STM32F4xx
    #    stm32f4xx.h, stm32f411xe.h, system_stm32f4xx.h/c, startup
    # ------------------------------------------------------------------
    FetchContent_Declare(
        cmsis_device_f4
        GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_device_f4.git
        GIT_TAG        master
        GIT_SHALLOW    ON
    )

    # ------------------------------------------------------------------
    # 3. CMSIS-Driver — обновлённые заголовки ARM Driver_GPIO.h и др.
    # ------------------------------------------------------------------
    FetchContent_Declare(
        cmsis_driver
        GIT_REPOSITORY https://github.com/ARM-software/CMSIS-Driver.git
        GIT_TAG        2.9.0
        GIT_SHALLOW    ON
    )

    # Скачиваем все три пакета
    FetchContent_MakeAvailable(cmsis5 cmsis_device_f4 cmsis_driver)

    # ------------------------------------------------------------------
    # Проверяем что скачалось
    # ------------------------------------------------------------------
    foreach(_check
        "${cmsis5_SOURCE_DIR}/CMSIS/Core/Include/core_cm4.h"
        "${cmsis_device_f4_SOURCE_DIR}/Include/stm32f4xx.h"
        "${cmsis_device_f4_SOURCE_DIR}/Source/Templates/system_stm32f4xx.c"
    )
        if(NOT EXISTS "${_check}")
            message(WARNING "AUTO_FETCH: файл не найден: ${_check}")
        else()
            message(STATUS "AUTO_FETCH: OK → ${_check}")
        endif()
    endforeach()

    # ------------------------------------------------------------------
    # Пути включения из скачанных пакетов
    # ------------------------------------------------------------------
    set(FETCH_INCLUDES
        # CMSIS-Core (core_cm4.h, cmsis_gcc.h ...)
        ${cmsis5_SOURCE_DIR}/CMSIS/Core/Include

        # STM32F4xx Device headers (stm32f4xx.h, stm32f411xe.h ...)
        ${cmsis_device_f4_SOURCE_DIR}/Include

        # Driver headers из CMSIS-Driver repo (Driver_GPIO.h, Driver_SPI.h ...)
        ${cmsis_driver_SOURCE_DIR}/Include

        # Локальные Driver headers (fallback + наш Driver_GPIO.h с CMSIS 6 API)
        ${CMAKE_SOURCE_DIR}/CMSIS/Driver/Include
    )

    # ------------------------------------------------------------------
    # Исходники из скачанных пакетов
    # ------------------------------------------------------------------
    set(FETCH_SOURCES
        # system_stm32f4xx.c из ST Device pack
        ${cmsis_device_f4_SOURCE_DIR}/Source/Templates/system_stm32f4xx.c
    )

    message(STATUS "AUTO_FETCH: CMSIS_5         → ${cmsis5_SOURCE_DIR}/CMSIS")
    message(STATUS "AUTO_FETCH: cmsis_device_f4 → ${cmsis_device_f4_SOURCE_DIR}")
    message(STATUS "AUTO_FETCH: CMSIS-Driver    → ${cmsis_driver_SOURCE_DIR}")

else()
    # Режим без загрузки — только локальные файлы
    message(STATUS "AUTO_FETCH выключен — используются локальные CMSIS файлы")
    set(FETCH_INCLUDES "")
    set(FETCH_SOURCES
        ${CMAKE_SOURCE_DIR}/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
    )
endif()










###################### VARIABLES ######################################
# Основные параметры проекта
set(PROJECT_NAME             "F411CEU6_SPI_ARM_driver")
set(PROJECT_TYPE             "exe")
set(LINKER_SCRIPT            "../STM32F411CEUX_FLASH.ld")
set(MCPU                     ${MCPU_CORTEX_M4})
set(MFPU                     ${MFPU_FPV4_SP_D16})
set(MFLOAT_ABI              ${MFLOAT_ABI_HARDWARE})  # hard FPU — как CubeIDE по умолчанию для M4
set(RUNTIME_LIBRARY          ${RUNTIME_LIBRARY_REDUCED_C})
set(RUNTIME_LIBRARY_SYSCALLS ${RUNTIME_LIBRARY_SYSCALLS_MINIMAL})

# Настройки OpenOCD
set(OPENOCD_INTERFACE "interface/stlink.cfg")
set(OPENOCD_TARGET    "target/stm32f4x.cfg")

# Исходные файлы проекта
set(PROJECT_SOURCES
    # Системные файлы
    Startup/startup_stm32f411ceux.s
    Sources/main.c
    Sources/syscalls.c
    Sources/sysmem.c

    # system_stm32f4xx.c — из FetchContent или локальный (AUTO_FETCH=OFF)
    ${FETCH_SOURCES}

    # Файлы графики и интерфейса
 #  Sources/GFX/GFX_FUNCTIONS.c
 #  Sources/Fonts/fonts.c
 #  Sources/ST7735/ST7735.c

    # CMSIS Driver файлы — раскомментируйте нужные
    # CMSIS/Driver/ST/STM32F4xx/Source/SPI_STM32F4xx.c
    # CMSIS/Driver/ST/STM32F4xx/Source/USART_STM32F4xx.c
    # CMSIS/Driver/ST/STM32F4xx/Source/I2C_STM32F4xx.c
    CMSIS/Driver/ST/STM32F4xx/Source/GPIO_STM32F4xx.c

    # E-Paper 2.13" драйвер (WeAct Studio, SSD1680)
    Sources/EPD/epd213.c

    # Dallas DS1620 3-wire термометр (PB3=CLK, PB4=DQ, PB5=RST)
    Sources/DS1620/ds1620.c
)

# Определения компилятора
set(PROJECT_DEFINES
    STM32F411xE             # Определяем тип микроконтроллера
    USE_CMSIS_DRIVER        # Включаем поддержку CMSIS Driver
    # Добавьте дополнительные определения при необходимости
)

# Пути включения заголовочных файлов
set(PROJECT_INCLUDES
    Sources
#   Sources/ST7735
    Sources/Inc

    # Локальные CMSIS заголовки (fallback если AUTO_FETCH=OFF)
    CMSIS/Core/Include
    CMSIS/Device/ST/STM32F4xx/Include
    CMSIS/Driver/Include
    CMSIS/Driver/ST/STM32F4xx/Include

    # Заголовки EPD драйвера
    Sources/EPD

    # Заголовки DS1620 драйвера
    Sources/DS1620

    # Конфигурация RTE
    CMSIS/Config

    # Заголовки из FetchContent (приоритет выше — добавляются первыми)
    ${FETCH_INCLUDES}
)

################## PROJECT SETUP ######################################
# project() и enable_language() объявлены в начале файла (до FetchContent)

# Создание цели сборки
if(${PROJECT_TYPE} MATCHES ${PROJECT_TYPE_EXECUTABLE})
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
elseif(${PROJECT_TYPE} MATCHES ${PROJECT_TYPE_STATIC_LIBRARY})
    add_library(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

# Добавление определений и путей включения
add_compile_definitions(${PROJECT_DEFINES})
include_directories(${PROJECT_INCLUDES})

# Настройка флагов компиляции
set(CMAKE_C_FLAGS   "${MCPU} -std=gnu11 ${MFPU} ${MFLOAT_ABI} ${RUNTIME_LIBRARY} -mthumb -Wall -Werror")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS "${MCPU} -mthumb ${MFPU} ${MFLOAT_ABI} -T${LINKER_SCRIPT} ${RUNTIME_LIBRARY_SYSCALLS} -Wl,-Map=${PROJECT_NAME}.map -Wl,--gc-sections -static -Wl,--start-group -lc -lm -Wl,--end-group")

# Установка суффиксов выходных файлов
set(CMAKE_EXECUTABLE_SUFFIX ".elf")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")

################## CUSTOM TARGETS ######################################
# Цель для очистки
add_custom_target(custom_clean
    COMMAND rm -f *.o *.elf *.map *.hex *.bin
)

# Генерация hex и bin файлов
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
)

# Команда для прошивки через OpenOCD
add_custom_target(flash
    DEPENDS ${PROJECT_NAME}
    COMMAND /usr/local/bin/openocd 
        -f ${OPENOCD_INTERFACE}
        -f ${OPENOCD_TARGET}
        -c "init"
        -c "reset halt"
        -c "flash write_image erase ${PROJECT_NAME}.hex"
        -c "reset"
        -c "exit"
)

# Команда для очистки флеш-памяти
add_custom_target(erase
    COMMAND /usr/local/bin/openocd 
        -f ${OPENOCD_INTERFACE}
        -f ${OPENOCD_TARGET}
        -c "init"
        -c "reset halt"
        -c "flash erase_sector 0 0 last"
        -c "exit"
)

# Команда для сброса микроконтроллера
add_custom_target(reset
    COMMAND /usr/local/bin/openocd 
        -f ${OPENOCD_INTERFACE}
        -f ${OPENOCD_TARGET}
        -c "init"
        -c "reset"
        -c "exit"
)

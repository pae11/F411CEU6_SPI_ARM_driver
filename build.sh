#!/bin/bash
#
# =============================================================================
# build.sh — скрипт сборки проекта F411CEU6_SPI_ARM_driver
# =============================================================================
#
# Использование:
#   ./build.sh              — полная пересборка (AUTO_FETCH=ON)
#   ./build.sh --no-fetch   — сборка без скачивания пакетов (AUTO_FETCH=OFF)
#
# FetchContent — пакеты скачиваются автоматически при первой сборке:
# ┌────────────────────┬─────────────────────────────────────────────────────┐
# │ Имя                │ Репозиторий / Тег                                   │
# ├────────────────────┼─────────────────────────────────────────────────────┤
# │ cmsis5             │ ARM-software/CMSIS_5 @ 5.9.0                        │
# │                    │ → CMSIS/Core/Include (core_cm4.h, cmsis_gcc.h ...)  │
# ├────────────────────┼─────────────────────────────────────────────────────┤
# │ cmsis_device_f4    │ STMicroelectronics/cmsis_device_f4 @ master         │
# │                    │ → Include/stm32f4xx.h, stm32f411xe.h ...            │
# │                    │ → Source/Templates/system_stm32f4xx.c               │
# ├────────────────────┼─────────────────────────────────────────────────────┤
# │ cmsis_driver       │ ARM-software/CMSIS-Driver @ 2.9.0                   │
# │                    │ → Include/Driver_GPIO.h, Driver_SPI.h ...           │
# └────────────────────┴─────────────────────────────────────────────────────┘
#
# Кэш пакетов хранится в Debug/_deps и сохраняется между пересборками.
# При rm -rf Debug скрипт перекладывает _deps во временную папку и
# восстанавливает после очистки — повторного скачивания не происходит.
#
# =============================================================================
set -e

# ─── Найти cmake ────────────────────────────────────────────────────────────
CMAKE_BIN=""
for p in \
    "$(which cmake 2>/dev/null)" \
    /opt/homebrew/bin/cmake \
    /usr/local/bin/cmake \
    /usr/bin/cmake; do
    if [ -x "$p" ]; then
        CMAKE_BIN="$p"
        break
    fi
done

if [ -z "$CMAKE_BIN" ]; then
    echo "Ошибка: cmake не найден. Установите: brew install cmake"
    exit 1
fi
echo "Используем cmake: $CMAKE_BIN ($(${CMAKE_BIN} --version | head -1))"

# ─── Параметры ───────────────────────────────────────────────────────────────
BUILD_DIR="Debug"
TOOLCHAIN="$(pwd)/toolchain.cmake"

# Опция: отключить авто-загрузку пакетов
# ./build.sh --no-fetch
AUTO_FETCH_OPT="-DAUTO_FETCH=ON"
for arg in "$@"; do
    [ "$arg" = "--no-fetch" ] && AUTO_FETCH_OPT="-DAUTO_FETCH=OFF"
done

# ─── Умная очистка: сохраняем _deps чтобы не скачивать пакеты повторно ──────
if [ -d "$BUILD_DIR" ]; then
    echo "Очищаем $BUILD_DIR (сохраняем _deps кэш)..."
    # Сохраняем скачанные пакеты
    if [ -d "$BUILD_DIR/_deps" ]; then
        mv "$BUILD_DIR/_deps" /tmp/_deps_cache_$$
    fi
    rm -rf "$BUILD_DIR"
    mkdir "$BUILD_DIR"
    # Восстанавливаем кэш пакетов
    if [ -d /tmp/_deps_cache_$$ ]; then
        mv /tmp/_deps_cache_$$ "$BUILD_DIR/_deps"
    fi
else
    mkdir "$BUILD_DIR"
fi

# ─── cmake конфигурация ──────────────────────────────────────────────────────
echo "Запускаем cmake конфигурацию..."
"$CMAKE_BIN" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    "$AUTO_FETCH_OPT" \
    -S . \
    -B "$BUILD_DIR" \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Debug

# ─── Сборка ──────────────────────────────────────────────────────────────────
echo "Собираем проект..."
make -C "$BUILD_DIR" -j$(sysctl -n hw.logicalcpu)

echo ""
echo "✅ Сборка успешно завершена!"
echo "   ELF : $BUILD_DIR/F411CEU6_SPI_ARM_driver.elf"
echo "   HEX : $BUILD_DIR/F411CEU6_SPI_ARM_driver.hex"
echo "   BIN : $BUILD_DIR/F411CEU6_SPI_ARM_driver.bin"
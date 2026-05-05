# STM32F411 FreeRTOS Bare-Metal Project

A bare-metal FreeRTOS implementation for the STM32F411CEU6 (Black Pill) featuring a synchronized UART CLI for hardware control and diagnostics.

## Features
- **FreeRTOS Kernel:** Multitasking with synchronized UART output.
- **Interactive CLI:** 
  - `help`: List available commands.
  - `blink on/off`: Control onboard LED (PC13).
  - `blink freq <ms>`: Adjust LED toggle frequency.
  - `clock`: Display system clock source and AHB prescaler.
  - `i2c detect`: Scan I2C1 bus (PB6/PB7) for connected devices.
- **Robust UART:** Interrupt-driven reception with a command buffer and thread-safe echoing.
- **Bare-Metal:** Custom linker script and startup code (no HAL/LL libraries).

## Hardware Configuration
- **MCU:** STM32F411CEU6 (Cortex-M4, 512KB Flash, 128KB RAM)
- **LED:** PC13 (Active Low)
- **USART1:** 
  - TX: PA9
  - RX: PA10
  - Baud: 115200
- **I2C1:**
  - SCL: PB6
  - SDA: PB7
  - Speed: 100kHz

## Build and Flash

### Prerequisites
- `arm-none-eabi-gcc` toolchain.
- `st-link` tools (`st-flash`).
- FreeRTOS Kernel source (expected at `../FreeRTOS-Kernel`).

### Instructions
1. **Build:**
   ```bash
   make
   ```
2. **Flash:**
   ```bash
   make flash
   ```
3. **Clean:**
   ```bash
   make clean
   ```

## Project Structure
- `main.c`: Application logic, CLI tasks, and register-level drivers.
- `startup.s`: Vector table, FPU enable, and data/bss initialization.
- `linker.ld`: Memory map and section placement.
- `FreeRTOSConfig.h`: Kernel settings and handler mapping.
- `Makefile`: Build rules and flashing targets.

## License
MIT

TARGET = firmware

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

FREERTOS_KERNEL_PATH = ../FreeRTOS-Kernel

CFLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
         -O2 -ffreestanding -Wall -g

LDFLAGS = -Tlinker.ld -nostartfiles -Wl,--gc-sections

INCLUDES = -I. \
           -I$(FREERTOS_KERNEL_PATH)/include \
           -I$(FREERTOS_KERNEL_PATH)/portable/GCC/ARM_CM4F

FREERTOS_SRC = \
    $(FREERTOS_KERNEL_PATH)/tasks.c \
    $(FREERTOS_KERNEL_PATH)/queue.c \
    $(FREERTOS_KERNEL_PATH)/list.c \
    $(FREERTOS_KERNEL_PATH)/timers.c \
    $(FREERTOS_KERNEL_PATH)/portable/GCC/ARM_CM4F/port.c \
    $(FREERTOS_KERNEL_PATH)/portable/MemMang/heap_4.c

SRCS = main.c startup.s $(FREERTOS_SRC)

# Put objects in current directory to avoid writing to kernel dir
OBJS = main.o startup.o tasks.o queue.o list.o timers.o port.o heap_4.o

all: $(TARGET).bin

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

flash: $(TARGET).bin
	st-flash write $(TARGET).bin 0x8000000

clean:
	rm -f *.o $(TARGET).elf $(TARGET).bin

main.o: main.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

startup.o: startup.s
	$(CC) $(CFLAGS) -c $< -o $@

tasks.o: $(FREERTOS_KERNEL_PATH)/tasks.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

queue.o: $(FREERTOS_KERNEL_PATH)/queue.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

list.o: $(FREERTOS_KERNEL_PATH)/list.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

timers.o: $(FREERTOS_KERNEL_PATH)/timers.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

port.o: $(FREERTOS_KERNEL_PATH)/portable/GCC/ARM_CM4F/port.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

heap_4.o: $(FREERTOS_KERNEL_PATH)/portable/MemMang/heap_4.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

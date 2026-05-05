#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Registers */
#define RCC_AHB1ENR (*(volatile uint32_t*)0x40023830)
#define RCC_APB2ENR (*(volatile uint32_t*)0x40023844)
#define RCC_CFGR    (*(volatile uint32_t*)0x40023808)

#define GPIOC_MODER (*(volatile uint32_t*)0x40020800)
#define GPIOC_ODR   (*(volatile uint32_t*)0x40020814)

#define GPIOA_MODER (*(volatile uint32_t*)0x40020000)
#define GPIOA_AFRH  (*(volatile uint32_t*)0x40020024)

#define USART1_SR   (*(volatile uint32_t*)0x40011000)
#define USART1_DR   (*(volatile uint32_t*)0x40011004)
#define USART1_BRR  (*(volatile uint32_t*)0x40011008)
#define USART1_CR1  (*(volatile uint32_t*)0x4001100C)

#define NVIC_ISER1  (*(volatile uint32_t*)0xE000E104)

/* Queue & State */
QueueHandle_t uart_q;
QueueHandle_t rx_q;
volatile int blink_enabled = 1;
volatile int blink_freq_ms = 500;

/* UART Interrupt Handler */
void USART1_IRQHandler(void) {
    if (USART1_SR & (1 << 5)) { // RXNE
        uint8_t c = (uint8_t)USART1_DR;
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(rx_q, &c, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
}

/* Helpers */
void uart_send_byte(uint8_t c) {
    while (!(USART1_SR & (1 << 7)));
    USART1_DR = c;
}

void uart_print(const char *s) {
    while (*s) {
        uint8_t c = *s++;
        xQueueSend(uart_q, &c, portMAX_DELAY);
    }
}

void print_num(int n) {
    char buf[12];
    int i = 0;
    if (n == 0) {
        uart_print("0");
        return;
    }
    if (n < 0) {
        uart_print("-");
        n = -n;
    }
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (i > 0) {
        uint8_t c = buf[--i];
        xQueueSend(uart_q, &c, portMAX_DELAY);
    }
}

/* Tasks */
void uart_task(void *arg) {
    uint8_t c;
    while (1) {
        if (xQueueReceive(uart_q, &c, portMAX_DELAY)) {
            uart_send_byte(c);
        }
    }
}

void process_cmd(char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        uart_print("Commands:\r\n");
        uart_print(" help           - Show this help\r\n");
        uart_print(" blink on       - Enable LED blink\r\n");
        uart_print(" blink off      - Disable LED blink\r\n");
        uart_print(" blink freq <ms>- Set blink frequency\r\n");
        uart_print(" clock          - Show clock info\r\n");
    } else if (strcmp(cmd, "blink on") == 0) {
        blink_enabled = 1;
        uart_print("Blink enabled\r\n");
    } else if (strcmp(cmd, "blink off") == 0) {
        blink_enabled = 0;
        GPIOC_ODR |= (1 << 13); // Turn off (PC13 High)
        uart_print("Blink disabled\r\n");
    } else if (strncmp(cmd, "blink freq ", 11) == 0) {
        int f = atoi(cmd + 11);
        if (f > 0) {
            blink_freq_ms = f;
            uart_print("Freq set to ");
            print_num(f);
            uart_print(" ms\r\n");
        }
    } else if (strcmp(cmd, "clock") == 0) {
        uint32_t cfgr = RCC_CFGR;
        uint32_t sws = (cfgr >> 2) & 0x3;
        uart_print("System Clock Source: ");
        if (sws == 0) uart_print("HSI (16MHz)");
        else if (sws == 1) uart_print("HSE");
        else if (sws == 2) uart_print("PLL");
        
        uint32_t hpre = (cfgr >> 4) & 0xF;
        uart_print("\r\nAHB Prescaler: ");
        if (hpre < 8) uart_print("1");
        else {
            int div = 1 << (hpre - 7);
            print_num(div);
        }
        uart_print("\r\n");
    } else if (strlen(cmd) > 0) {
        uart_print("Unknown cmd: ");
        uart_print(cmd);
        uart_print("\r\n");
    }
}

void cli_task(void *arg) {
    char buf[64];
    int idx = 0;
    uint8_t c;
    uint8_t last_c = 0;

    uart_print("\r\n--- STM32F411 FreeRTOS CLI ---\r\n");
    uart_print("STM32 CLI> ");
    while (1) {
        if (xQueueReceive(rx_q, &c, portMAX_DELAY)) {
            /* Handle \r\n or \n\r as a single newline */
            if ((c == '\n' && last_c == '\r') || (c == '\r' && last_c == '\n')) {
                last_c = 0;
                continue;
            }
            last_c = c;

            if (c == '\r' || c == '\n') {
                uart_print("\r\n");
                buf[idx] = '\0';
                process_cmd(buf);
                idx = 0;
                uart_print("STM32 CLI> ");
            } else if (c == 127 || c == 8) { // Backspace
                if (idx > 0) {
                    idx--;
                    uart_print("\b \b");
                }
            } else if (idx < 63) {
                buf[idx++] = c;
                /* Echo back using the UART queue */
                xQueueSend(uart_q, &c, portMAX_DELAY);
            }
        }
    }
}

void led_task(void *arg) {
    while (1) {
        if (blink_enabled) {
            GPIOC_ODR ^= (1 << 13);
        }
        vTaskDelay(pdMS_TO_TICKS(blink_freq_ms));
    }
}

/* Init */
void gpio_init() {
    /* Enable GPIOA and GPIOC clock */
    RCC_AHB1ENR |= (1 << 0) | (1 << 2);

    /* PC13 output (LED) */
    GPIOC_MODER &= ~(3 << (13 * 2));
    GPIOC_MODER |=  (1 << (13 * 2));

    /* PA9 TX AF7, PA10 RX AF7 */
    GPIOA_MODER &= ~((3 << (9 * 2)) | (3 << (10 * 2)));
    GPIOA_MODER |=  ((2 << (9 * 2)) | (2 << (10 * 2)));

    GPIOA_AFRH &= ~((0xF << 4) | (0xF << 8));
    GPIOA_AFRH |=  ((7 << 4) | (7 << 8));
}

void uart_init() {
    /* Enable USART1 clock */
    RCC_APB2ENR |= (1 << 4);
    /* 115200 baud @ 16MHz HSI */
    USART1_BRR = 139;
    /* USART enable, TX enable, RX enable, RXNE interrupt enable */
    USART1_CR1 = (1 << 13) | (1 << 3) | (1 << 2) | (1 << 5);

    /* Enable USART1 in NVIC (IRQ 37) */
    NVIC_ISER1 |= (1 << (37 - 32));
}

int main(void) {
    gpio_init();
    uart_init();

    uart_q = xQueueCreate(64, sizeof(uint8_t));
    rx_q   = xQueueCreate(64, sizeof(uint8_t));

    xTaskCreate(led_task, "LED", 128, 0, 1, 0);
    xTaskCreate(uart_task, "UART_TX", 128, 0, 2, 0);
    xTaskCreate(cli_task, "CLI", 256, 0, 1, 0);

    vTaskStartScheduler();

    while (1);
}

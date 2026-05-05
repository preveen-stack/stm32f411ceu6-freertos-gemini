#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Registers */
#define RCC_AHB1ENR (*(volatile uint32_t*)0x40023830)
#define RCC_APB2ENR (*(volatile uint32_t*)0x40023844)
#define RCC_APB1ENR (*(volatile uint32_t*)0x40023840)
#define RCC_CFGR    (*(volatile uint32_t*)0x40023808)

#define GPIOB_MODER (*(volatile uint32_t*)0x40020400)
#define GPIOB_OTYPER (*(volatile uint32_t*)0x40020404)
#define GPIOB_AFRL  (*(volatile uint32_t*)0x40020420)

#define GPIOC_MODER (*(volatile uint32_t*)0x40020800)
#define GPIOC_ODR   (*(volatile uint32_t*)0x40020814)

#define GPIOA_MODER (*(volatile uint32_t*)0x40020000)
#define GPIOA_AFRH  (*(volatile uint32_t*)0x40020024)

#define USART1_SR   (*(volatile uint32_t*)0x40011000)
#define USART1_DR   (*(volatile uint32_t*)0x40011004)
#define USART1_BRR  (*(volatile uint32_t*)0x40011008)
#define USART1_CR1  (*(volatile uint32_t*)0x4001100C)

#define I2C1_CR1    (*(volatile uint32_t*)0x40005400)
#define I2C1_CR2    (*(volatile uint32_t*)0x40005404)
#define I2C1_DR     (*(volatile uint32_t*)0x40005410)
#define I2C1_SR1    (*(volatile uint32_t*)0x40005414)
#define I2C1_SR2    (*(volatile uint32_t*)0x40005418)
#define I2C1_CCR    (*(volatile uint32_t*)0x4000541C)
#define I2C1_TRISE  (*(volatile uint32_t*)0x40005420)

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

void print_hex(uint8_t n) {
    const char *hex = "0123456789ABCDEF";
    uint8_t c1 = hex[(n >> 4) & 0xF];
    uint8_t c2 = hex[n & 0xF];
    xQueueSend(uart_q, &c1, portMAX_DELAY);
    xQueueSend(uart_q, &c2, portMAX_DELAY);
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

void i2c_init() {
    /* Enable GPIOB clock */
    RCC_AHB1ENR |= (1 << 1);
    
    /* PB6 SCL AF4, PB7 SDA AF4 */
    GPIOB_MODER &= ~((3 << (6 * 2)) | (3 << (7 * 2)));
    GPIOB_MODER |=  ((2 << (6 * 2)) | (2 << (7 * 2)));
    
    /* Open-drain */
    GPIOB_OTYPER |= ((1 << 6) | (1 << 7));
    
    /* AF4 for I2C1 */
    GPIOB_AFRL &= ~((0xF << 24) | (0xF << 28));
    GPIOB_AFRL |=  ((4 << 24) | (4 << 28));
    
    /* Enable I2C1 clock */
    RCC_APB1ENR |= (1 << 21);
    
    /* Reset I2C */
    I2C1_CR1 |= (1 << 15);
    I2C1_CR1 &= ~(1 << 15);
    
    /* Peripheral clock 16MHz */
    I2C1_CR2 = 16;
    
    /* 100kHz standard mode */
    I2C1_CCR = 80;
    I2C1_TRISE = 17;
    
    /* Enable I2C */
    I2C1_CR1 |= (1 << 0);
}

void process_i2c_detect() {
    uart_print("Scanning I2C1 (PB6/PB7)...\r\n");
    uart_print("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    
    for (uint8_t i = 0; i < 128; i += 16) {
        print_hex(i);
        uart_print(":");
        for (uint8_t j = 0; j < 16; j++) {
            uint8_t addr = i + j;
            if (addr == 0 || addr > 127) {
                uart_print("   ");
                continue;
            }
            
            /* Wait for bus not busy */
            int timeout = 1000;
            while ((I2C1_SR2 & (1 << 1)) && timeout--);
            
            /* Start */
            I2C1_CR1 |= (1 << 8);
            timeout = 1000;
            while (!(I2C1_SR1 & (1 << 0)) && timeout--);
            
            /* Send Address */
            (void)I2C1_SR1;
            I2C1_DR = (addr << 1);
            
            timeout = 1000;
            int found = 0;
            while (timeout--) {
                uint32_t sr1 = I2C1_SR1;
                if (sr1 & (1 << 1)) { // ADDR set -> ACK
                    found = 1;
                    (void)I2C1_SR1;
                    (void)I2C1_SR2;
                    break;
                }
                if (sr1 & (1 << 10)) { // AF set -> NACK
                    I2C1_SR1 &= ~(1 << 10);
                    break;
                }
            }
            
            /* Stop */
            I2C1_CR1 |= (1 << 9);
            
            if (found) {
                uart_print(" ");
                print_hex(addr);
            } else {
                uart_print(" --");
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        uart_print("\r\n");
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
        uart_print(" i2c detect     - Scan I2C1 bus\r\n");
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
    } else if (strcmp(cmd, "i2c detect") == 0) {
        process_i2c_detect();
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
    i2c_init();

    uart_q = xQueueCreate(64, sizeof(uint8_t));
    rx_q   = xQueueCreate(64, sizeof(uint8_t));

    xTaskCreate(led_task, "LED", 128, 0, 1, 0);
    xTaskCreate(uart_task, "UART_TX", 128, 0, 2, 0);
    xTaskCreate(cli_task, "CLI", 256, 0, 1, 0);

    vTaskStartScheduler();

    while (1);
}

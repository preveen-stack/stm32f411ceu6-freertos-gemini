#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Registers */
#define RCC_AHB1ENR (*(volatile uint32_t*)0x40023830)
#define RCC_APB2ENR (*(volatile uint32_t*)0x40023844)

#define GPIOC_MODER (*(volatile uint32_t*)0x40020800)
#define GPIOC_ODR   (*(volatile uint32_t*)0x40020814)

#define GPIOA_MODER (*(volatile uint32_t*)0x40020000)
#define GPIOA_AFRH  (*(volatile uint32_t*)0x40020024)

#define USART1_SR   (*(volatile uint32_t*)0x40011000)
#define USART1_DR   (*(volatile uint32_t*)0x40011004)
#define USART1_BRR  (*(volatile uint32_t*)0x40011008)
#define USART1_CR1  (*(volatile uint32_t*)0x4001100C)

/* Queue */
QueueHandle_t uart_q;

/* Init */
void gpio_init() {
    /* Enable GPIOA and GPIOC clock */
    RCC_AHB1ENR |= (1 << 0) | (1 << 2);

    /* PC13 output (LED) */
    GPIOC_MODER &= ~(3 << (13 * 2));
    GPIOC_MODER |=  (1 << (13 * 2));

    /* PA9 TX AF7 */
    GPIOA_MODER &= ~(3 << (9 * 2));
    GPIOA_MODER |=  (2 << (9 * 2));

    GPIOA_AFRH &= ~(0xF << 4);
    GPIOA_AFRH |=  (7 << 4);
}

void uart_init() {
    /* Enable USART1 clock */
    RCC_APB2ENR |= (1 << 4);
    /* 115200 baud @ 16MHz HSI */
    USART1_BRR = 139;
    /* USART enable, TX enable */
    USART1_CR1 = (1 << 13) | (1 << 3);
}

void uart_send_byte(uint8_t c) {
    while (!(USART1_SR & (1 << 7)));
    USART1_DR = c;
}

void uart_task(void *arg) {
    uint8_t c;
    while (1) {
        if (xQueueReceive(uart_q, &c, portMAX_DELAY)) {
            uart_send_byte(c);
        }
    }
}

void uart_print(const char *s) {
    while (*s) {
        uint8_t c = *s++;
        xQueueSend(uart_q, &c, portMAX_DELAY);
    }
}

/* LED task */
void led_task(void *arg) {
    while (1) {
        GPIOC_ODR ^= (1 << 13);
        uart_print("Blink\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void) {
    gpio_init();
    uart_init();

    uart_q = xQueueCreate(64, sizeof(uint8_t));

    uart_print("FreeRTOS STM32F411 Starting...\r\n");

    xTaskCreate(led_task, "LED", 128, 0, 1, 0);
    xTaskCreate(uart_task, "UART", 128, 0, 2, 0);

    vTaskStartScheduler();

    while (1);
}

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"
#include "ssd1306.h"
#include "hardware/adc.h"
#include "hardware/uart.h"

QueueHandle_t xQueueADC;

const int X_PIN = 27;
const int Y_PIN = 26;

typedef struct adc {
    int axis;
    int val;
} adc_t;

void x_task(void *p) {
    adc_t x_data;
    
    adc_init();
    adc_gpio_init(X_PIN);
    
    // ==== media movel ====
    int buffer[5] = {0, 0, 0, 0, 0};
    int buffer_index = 0;
    int sum = 0;
    // =====================
    
    while (1) {
        float result;
        int read;
        adc_select_input(1);
        read = (adc_read() - 2047) / 8.0;

        // ==== media movel ====
        buffer[buffer_index] = read;
        buffer_index++;

        for (int j = 0; j < 5; j++) {
            sum += buffer[j];
        }
        sum /= 5;

        result = sum;

        sum = 0;
        if (buffer_index > 4) buffer_index = 0;
        // ======================

        if (result > -50 && result < 50) result = 0;
        
        x_data.axis = 0;
        x_data.val = (int)result;

        if (result) xQueueSend(xQueueADC, &x_data, 0);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void y_task(void *p) {
    adc_t y_data;
    
    adc_init();
    adc_gpio_init(Y_PIN);
    
    // ==== media movel ====
    int buffer[5] = {0, 0, 0, 0, 0};
    int buffer_index = 0;
    int sum = 0;
    // =====================
    
    while (1) {
        float result;
        int read;
        adc_select_input(0);
        read = (adc_read() - 2047) / 8.0;

        // ==== media movel ====
        buffer[buffer_index] = read;
        buffer_index++;

        for (int j = 0; j < 5; j++) {
            sum += buffer[j];
        }
        sum /= 5;

        result = sum;

        sum = 0;
        if (buffer_index > 4) buffer_index = 0;
        // ======================
        
        if (result > -50 && result < 50) result = 0;
        
        y_data.axis = 1;
        y_data.val = (int)result;

        if (result) xQueueSend(xQueueADC, &y_data, 0);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void uart_task(void *p) {
    adc_t data;

    while (1) {
        if (xQueueReceive(xQueueADC, &data, pdMS_TO_TICKS(100))) {
            uart_putc_raw(uart0, data.axis);
            uart_putc_raw(uart0, data.val);
            uart_putc_raw(uart0, data.val >> 8);
            uart_putc_raw(uart0, -1);
        }        
    }
}

int main() {
    stdio_init_all();

    xQueueADC = xQueueCreate(3, sizeof(adc_t));
    xTaskCreate(x_task, "X Task", 8192, NULL, 1, NULL);
    xTaskCreate(y_task, "Y Task", 8192, NULL, 1, NULL);
    xTaskCreate(uart_task, "UART Task", 8192, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true);
}

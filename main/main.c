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

// === Definições para SSD1306 ===
ssd1306_t disp;

QueueHandle_t xQueueBtn;

// == funcoes de inicializacao ===
void btn_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) xQueueSendFromISR(xQueueBtn, &gpio, 0);
}

void oled_display_init(void) {
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);
    ssd1306_show(&disp);
}

void btns_init(void) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);

    gpio_init(BTN_PIN_B);
    gpio_set_dir(BTN_PIN_B, GPIO_IN);
    gpio_pull_up(BTN_PIN_B);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R,
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true, &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_G, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                         true);
    gpio_set_irq_enabled(BTN_PIN_B, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                         true);
}

void led_rgb_init(void) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 1);

    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);
    gpio_put(LED_PIN_G, 1);

    gpio_init(LED_PIN_B);
    gpio_set_dir(LED_PIN_B, GPIO_OUT);
    gpio_put(LED_PIN_B, 1);
}
// ==============================================

void task_1(void *p) {
    btns_init();
    led_rgb_init();
    oled_display_init();

    uint btn_data;

    while (1) {
        if (xQueueReceive(xQueueBtn, &btn_data, pdMS_TO_TICKS(2000))) {
            printf("btn: %d \n", btn_data);

            switch (btn_data) {
                case BTN_PIN_B:
                    gpio_put(LED_PIN_B, 0);
                    ssd1306_draw_string(&disp, 8, 0, 2, "BLUE");
                    ssd1306_show(&disp);
                    break;
                case BTN_PIN_G:
                    gpio_put(LED_PIN_G, 0);
                    ssd1306_draw_string(&disp, 8, 24, 2, "GREEN");
                    ssd1306_show(&disp);
                    break;
                case BTN_PIN_R:
                    gpio_put(LED_PIN_R, 0);

                    ssd1306_draw_string(&disp, 8, 48, 2, "RED");
                    ssd1306_show(&disp);
                    break;
                default:
                    // Handle other buttons if needed
                    break;
            }
        } else {
            ssd1306_clear(&disp);
            ssd1306_show(&disp);
            gpio_put(LED_PIN_R, 1);
            gpio_put(LED_PIN_G, 1);
            gpio_put(LED_PIN_B, 1);
        }
    }
}

QueueHandle_t xQueueADC;

const int X_PIN = 27;
const int Y_PIN = 26;

typedef struct adc {
    int axis;
    int val;
} adc_t;

void x_task(void *p) {
    float result;
    adc_t x_data;
    
    adc_init();
    adc_gpio_init(X_PIN);
    
    // ==== media movel ====
    int buffer[5] = {0, 0, 0, 0, 0};
    int buffer_index = 0;
    int read = 0;
    int sum = 0;
    // =====================
    
    while (1) {
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
    float result;
    adc_t y_data;
    
    adc_init();
    adc_gpio_init(Y_PIN);
    
    // ==== media movel ====
    int buffer[5] = {0, 0, 0, 0, 0};
    int buffer_index = 0;
    int read = 0;
    int sum = 0;
    // =====================
    
    while (1) {
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

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "ssd1306.h"
#include "font.h"

#define LED_GREEN 11
#define LED_BLUE 12
#define LED_RED 13
#define BUTTON_A 5
#define BUTTON_JOYSTICK 22
#define JOYSTICK_X_PIN 26
#define JOYSTICK_Y_PIN 27
#define DEBOUNCE_TIME 200
volatile bool state_green_led = false;
volatile bool state_blue_led = false;
volatile bool state_red_led = false;
volatile uint32_t last_press_time_A = 0;
volatile uint32_t last_press_time_J = 0;
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define SSD1306_ADDR 0x3C
ssd1306_t ssd;
#define PWM_DIVISER 150.0
#define WRAP_VALUE 4096
volatile bool pwm_enabled = false;
volatile bool state_border = false;
volatile bool state_shape = false;
volatile int square_x = 60;
volatile int square_y = 28;
#define SQUARE_SIZE 8
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

void gpio_setup() {  // Configuração dos pinos GPIO
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);
    gpio_init(LED_RED);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(BUTTON_A);
    gpio_init(BUTTON_JOYSTICK);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_JOYSTICK, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_JOYSTICK);
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN); 
}

void pwm_setup() {  // Configuração do PWM para LEDs
    gpio_set_function(LED_BLUE, GPIO_FUNC_PWM);
    uint slice_led_blue = pwm_gpio_to_slice_num(LED_BLUE);
    pwm_set_clkdiv(slice_led_blue, PWM_DIVISER);
    pwm_set_wrap(slice_led_blue, WRAP_VALUE);
    pwm_set_enabled(slice_led_blue, true);
    gpio_set_function(LED_RED, GPIO_FUNC_PWM);
    uint slice_led_red = pwm_gpio_to_slice_num(LED_RED);
    pwm_set_clkdiv(slice_led_red, PWM_DIVISER);
    pwm_set_wrap(slice_led_red, WRAP_VALUE);
    pwm_set_enabled(slice_led_red, true);
}

void set_led_intensity(uint16_t joystick_x, uint16_t joystick_y) {  // Controla intensidade dos LEDs
    if (pwm_enabled) {
        uint16_t intensity_x = abs(joystick_x - 2048) * 2;
        uint16_t intensity_y = abs(joystick_y - 2048) * 2;
        pwm_set_gpio_level(LED_BLUE, intensity_y);
        pwm_set_gpio_level(LED_RED, intensity_x);
    } else {
        pwm_set_gpio_level(LED_BLUE, 0);
        pwm_set_gpio_level(LED_RED, 0);
    }
}


void update_display() {
    ssd1306_fill(&ssd, false);
    if (state_border) { 
        ssd1306_rect(&ssd, 0, 0, 128, 62, true, false); ssd1306_rect(&ssd, 2, 2, 124, 58, true, false); 
    }
    else {
        for (int i = 0; i < 128; i += 4) {
        ssd1306_pixel(&ssd, i, 0, true);     // Linha superior
        ssd1306_pixel(&ssd, i, 61, true);    // Linha inferior
        }
        for (int i = 0; i < 64; i += 4) {
        ssd1306_pixel(&ssd, 0, i, true);     // Linha esquerda
        ssd1306_pixel(&ssd, 127, i, true);   // Linha direita
        }
    }
    if (state_shape) {
        ssd1306_rect(&ssd, square_y, square_x, SQUARE_SIZE, SQUARE_SIZE, true, true);
    }

    
    ssd1306_send_data(&ssd);
}

void button_irq_handler(uint gpio, uint32_t events) {  // Callback das interrupções
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (gpio == BUTTON_A) {
        if (current_time - last_press_time_A > DEBOUNCE_TIME * 1000) {
            last_press_time_A = current_time;
            pwm_enabled = !pwm_enabled;
            state_shape = !state_shape;
            update_display();
        }
    } 
    else if (gpio == BUTTON_JOYSTICK) {
        if (current_time - last_press_time_J > DEBOUNCE_TIME * 1000) {
            last_press_time_J = current_time;
            state_green_led = !state_green_led;
            gpio_put(LED_GREEN, state_green_led);
            state_border = !state_border;
            update_display();
        }
    }
}

int main() {  
    stdio_init_all();
    gpio_setup();
    pwm_setup();
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, SSD1306_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    uint16_t joystick_x_position, joystick_y_position; 
    while (true) {
        adc_select_input(1);
        joystick_x_position = adc_read();
        adc_select_input(0);
        joystick_y_position = adc_read(); 
        set_led_intensity(joystick_x_position, joystick_y_position);
        if (state_shape) {
            square_x = (joystick_x_position * (SCREEN_WIDTH - SQUARE_SIZE)) / 4096;
            square_y = ((4096 - joystick_y_position) * (SCREEN_HEIGHT - SQUARE_SIZE)) / 4096;
            update_display();
        }
        sleep_ms(50);
    }
}
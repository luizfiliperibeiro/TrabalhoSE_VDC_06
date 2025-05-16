#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"
#include "pico/bootrom.h"

// --- Definições de pinos ---
#define JOY_X 26    // ADC0 (GPIO26)
#define JOY_Y 27    // ADC1 (GPIO27)
#define BOTAO_B 6   // GPIO6 (BOOTSEL)

// --- Struct para dados lidos ---
typedef struct {
    uint16_t nivel_agua;    // eixo Y
    uint16_t volume_chuva;  // eixo X
} sensor_data_t;

// --- Filas para comunicação entre tasks ---
QueueHandle_t xQueueSensorData;     // Fila para dados do joystick
QueueHandle_t xQueueSensorAlerta;   // Fila para alerta
QueueHandle_t xQueueSensorBuzzer;   // Fila para buzzer
QueueHandle_t xQueueSensorMatriz;   // Fila para matriz LED

// --- Leitura do joystick e envio para fila ---
void vJoystickTask(void *params) {
    adc_init();
    adc_gpio_init(JOY_Y);  // ADC0 (GPIO26)
    adc_gpio_init(JOY_X);  // ADC1 (GPIO27)

    sensor_data_t dados;

    while (true) {
        adc_select_input(0); // Y
        dados.nivel_agua = adc_read();

        adc_select_input(1); // X
        dados.volume_chuva = adc_read();

        xQueueSend(xQueueSensorData, &dados, 0);
        xQueueSend(xQueueSensorAlerta, &dados, 0);
        xQueueSend(xQueueSensorBuzzer, &dados, 0);
        xQueueSend(xQueueSensorMatriz, &dados, 0);
        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz
    }
}

#include "lib/ssd1306.h"
#include "lib/font.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define OLED_ADDR 0x3C

#define LIMITE_AGUA 2867    // 70% de 4095
#define LIMITE_CHUVA 3276   // 80% de 4095

void vDisplayTask(void *params) {      // Task para o display
    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // Configura o display
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, OLED_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    sensor_data_t dados;
    char buffer[32];

    while (true) {
        if (xQueueReceive(xQueueSensorData, &dados, portMAX_DELAY) == pdTRUE) {
            // Converte os valores em porcentagem
            uint8_t nivel_pct = (dados.nivel_agua * 100) / 4095;
            uint8_t chuva_pct = (dados.volume_chuva * 100) / 4095;

            // Verifica estado de alerta
            bool alerta = (dados.nivel_agua >= LIMITE_AGUA) || (dados.volume_chuva >= LIMITE_CHUVA);

            // Limpa display
            ssd1306_fill(&ssd, false);

            // Cabeçalho
            ssd1306_draw_string(&ssd, "EST. DE ALERTA", 5, 0);

            // Nível da água
            snprintf(buffer, sizeof(buffer), "N.Agua: %3d%%", nivel_pct);
            ssd1306_draw_string(&ssd, buffer, 0, 16);

            // Volume de chuva
            snprintf(buffer, sizeof(buffer), "V.Chuva: %3d%%", chuva_pct);
            ssd1306_draw_string(&ssd, buffer, 0, 28);

            // Modo
            if (alerta) {
                ssd1306_draw_string(&ssd, "MODO ALERTA!!!", 10, 45);
            } else {
                ssd1306_draw_string(&ssd, "Modo: Normal", 20, 45);
            }

            ssd1306_send_data(&ssd);
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

#define LED_R 13
#define LED_G 11
#define LED_B 12

// --- Definições de pinos para LED RGB ---
void init_led_rgb() {
    gpio_init(LED_R);
    gpio_init(LED_G);
    gpio_init(LED_B);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_set_dir(LED_B, GPIO_OUT);
}

// --- Função para definir a cor do LED RGB ---
void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    gpio_put(LED_R, r);
    gpio_put(LED_G, g);
    gpio_put(LED_B, b);
}

// --- Task para controle do LED RGB ---
void vLedRGBTask(void *params) {
    init_led_rgb();
    sensor_data_t dados;

    while (true) {
        if (xQueueReceive(xQueueSensorAlerta, &dados, portMAX_DELAY) == pdTRUE) {
            bool alerta = (dados.nivel_agua >= LIMITE_AGUA) || (dados.volume_chuva >= LIMITE_CHUVA);

            if (alerta) {
                set_rgb(1, 0, 0); // LED vermelho
            } else {
                set_rgb(0, 0, 0); // Desliga LED
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

#define BUZZER 21

// --- Definições de pinos para Buzzer ---
void init_buzzer() {
    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_put(BUZZER, 0); // Desliga inicialmente
}

// --- Função para emitir som no buzzer ---
void beep(uint ms_on, uint ms_off, uint8_t vezes) {
    for (uint8_t i = 0; i < vezes; i++) {
        gpio_put(BUZZER, 1);
        vTaskDelay(pdMS_TO_TICKS(ms_on));
        gpio_put(BUZZER, 0);
        vTaskDelay(pdMS_TO_TICKS(ms_off));
    }
}

// --- Task para controle do Buzzer ---
void vBuzzerTask(void *params) {
    init_buzzer();
    sensor_data_t dados;

    while (true) {
        if (xQueueReceive(xQueueSensorBuzzer, &dados, portMAX_DELAY) == pdTRUE) {
            bool alerta = (dados.nivel_agua >= LIMITE_AGUA) || (dados.volume_chuva >= LIMITE_CHUVA);

            if (alerta) {
                while (alerta) {
                    // Três bips curtos
                    for (int i = 0; i < 3; i++) {
                        gpio_put(BUZZER, 1);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        gpio_put(BUZZER, 0);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }

                    // Verifica novamente se ainda está em alerta
                    if (xQueueReceive(xQueueSensorBuzzer, &dados, 0) == pdTRUE) {
                        alerta = (dados.nivel_agua >= LIMITE_AGUA) || (dados.volume_chuva >= LIMITE_CHUVA);
                    } else {
                        alerta = true; // sem novos dados, presume que continua
                    }
                }

                // Saiu do alerta → garante que o buzzer fique em silêncio
                gpio_put(BUZZER, 0);
            } else {
                // Não está em alerta → permanece em silêncio
                gpio_put(BUZZER, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
    }
}

#include "ws2812.pio.h"
#include "lib/ws2812.h"

#define WS2812_PIN 7
#define NUM_PIXELS 25
#define IS_RGBW false

bool led_buffer[NUM_PIXELS] = {0};         // Buffer para LEDs
uint8_t led_r = 0, led_g = 0, led_b = 0;

PIO pio = pio0;     // PIO0
int sm = 0;

static inline void put_pixel(uint32_t pixel_grb) {   // Envia o pixel para o PIO
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {        // Converte RGB para formato GRB
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void set_led_buffer(const bool *figura) {   // Copia a figura para o buffer
    for (int i = 0; i < NUM_PIXELS; i++) {
        led_buffer[i] = figura[i];
    }
}

void atualizar_matriz() {           // Atualiza a matriz LED
    uint32_t cor = urgb_u32(led_r, led_g, led_b);
    for (int i = 0; i < NUM_PIXELS; i++) {
        if (led_buffer[i])
            put_pixel(cor);
        else
            put_pixel(0);
    }
}

// --- Triângulo vermelho ---
const bool triangulo_alerta[25] = {
    0,0,0,0,0,
    1,1,1,1,1,
    0,1,0,1,0,
    0,0,1,0,0,
    0,0,0,0,0
};

void vMatrizTask(void *params) {   // Task para controle da matriz LED
    sensor_data_t dados;

    while (true) {
        if (xQueueReceive(xQueueSensorMatriz, &dados, portMAX_DELAY) == pdTRUE) {
            bool alerta = (dados.nivel_agua >= LIMITE_AGUA) || (dados.volume_chuva >= LIMITE_CHUVA);

            if (alerta) {
                // Enquanto estiver em alerta, pisca
                while (alerta) {
                    // Triângulo vermelho
                    set_led_buffer(triangulo_alerta);
                    led_r = 100; led_g = 0; led_b = 0;
                    atualizar_matriz();
                    vTaskDelay(pdMS_TO_TICKS(500));

                    // Apaga
                    memset(led_buffer, 0, sizeof(led_buffer));
                    atualizar_matriz();
                    vTaskDelay(pdMS_TO_TICKS(500));

                    // Verifica se ainda está em alerta (nova leitura da fila)
                    if (xQueueReceive(xQueueSensorMatriz, &dados, 0) == pdTRUE) {
                        alerta = (dados.nivel_agua >= LIMITE_AGUA) || (dados.volume_chuva >= LIMITE_CHUVA);
                    } else {
                        // Caso não tenha novo dado, assume que continua
                        alerta = true;
                    }
                }

                // Saiu do alerta: apaga tudo
                memset(led_buffer, 0, sizeof(led_buffer));
                atualizar_matriz();
            } else {
                // Modo normal: apenas apaga e espera novo dado
                memset(led_buffer, 0, sizeof(led_buffer));
                atualizar_matriz();
                vTaskDelay(pdMS_TO_TICKS(300));
            }
        }
    }
}

// --- Função BOOTSEL via botão B ---
void gpio_irq_handler(uint gpio, uint32_t events) {
    reset_usb_boot(0, 0);
}

int main() {
    // Inicialização padrão
    stdio_init_all();

    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // Configuração BOOTSEL
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Criação das filas
    xQueueSensorData = xQueueCreate(5, sizeof(sensor_data_t));
    xQueueSensorAlerta = xQueueCreate(5, sizeof(sensor_data_t));
    xQueueSensorBuzzer = xQueueCreate(5, sizeof(sensor_data_t));
    xQueueSensorMatriz = xQueueCreate(5, sizeof(sensor_data_t));

    // Criação das tasks
    xTaskCreate(vJoystickTask, "JoystickTask", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "DisplayTask", 512, NULL, 1, NULL);
    xTaskCreate(vLedRGBTask, "LED RGB Task", 256, NULL, 1, NULL);
    xTaskCreate(vBuzzerTask, "BuzzerTask", 256, NULL, 1, NULL);
    xTaskCreate(vMatrizTask, "MatrizTask", 512, NULL, 1, NULL);

    // Inicia o escalonador
    vTaskStartScheduler();

    while (true); // nunca deve chegar aqui
}

// --- Fim do código ---
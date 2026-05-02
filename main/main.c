/**
 * @file main.c
 * @brief Aplicação principal do sistema de monitoramento de estufa
 *        Franzininho WiFi LAB01 – ESP-IDF v5.x
 *
 * Arquitetura de tarefas FreeRTOS:
 *
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  app_main()  ─── inicialização sequencial de todos os       │
 *   │                  componentes e criação das tarefas           │
 *   └─────────────────────────────────────────────────────────────┘
 *
 *   task_sensors  (core 0, prioridade 6)
 *     └─ lê DHT11 e LDR a cada SENSOR_READ_INTERVAL_MS
 *        atualiza s_state via mutex
 *
 *   task_control  (core 0, prioridade 5)
 *     └─ aplica lógica de histerese ao relé a cada 500 ms
 *
 *   task_display  (core 1, prioridade 4)
 *     └─ redesenha o OLED a cada 500 ms
 *
 *   task_logging  (core 0, prioridade 3)
 *     └─ grava log a cada LOG_INTERVAL_SEC segundos
 *
 *   serial_cmd (tarefa interna do componente, core 0, prioridade 5)
 *     └─ monitora UART0 e processa comandos
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

/* Componentes do projeto */
#include "dht11.h"
#include "ldr.h"
#include "oled_menu.h"
#include "data_logger.h"
#include "serial_cmd.h"

static const char *TAG = "MAIN";

/* ------------------------------------------------------------------ */
/* Estado global compartilhado                                         */
/* ------------------------------------------------------------------ */

static estufa_state_t s_state = {
    .temperature     = 0.0f,
    .humidity        = 0.0f,
    .luminosity      = 0,
    .setpoint        = CONFIG_SETPOINT_TEMP_DEFAULT,
    .control_enabled = true,
    .logging_enabled = CONFIG_LOGGING_ENABLED_DEFAULT,
    .relay_on        = false,
};

/* Mutex para acesso seguro ao estado entre tarefas */
static SemaphoreHandle_t s_state_mutex;

/* ------------------------------------------------------------------ */
/* Pinos de botões (GPIO)                                              */
/* ------------------------------------------------------------------ */

#define BTN_UP_GPIO   GPIO_NUM_0   /* BOOT button da Franzininho */
#define BTN_SEL_GPIO  GPIO_NUM_35  /* Botão de seleção adicional */
#define RELAY_GPIO    CONFIG_RELAY_GPIO

/* ------------------------------------------------------------------ */
/* Controle do relé                                                    */
/* ------------------------------------------------------------------ */

static void relay_set(bool on)
{
    gpio_set_level(RELAY_GPIO, on ? 1 : 0);
    s_state.relay_on = on;
}

/**
 * @brief Lógica de controle ON/OFF com histerese.
 *
 * Liga o relé quando temperatura < setpoint - histerese.
 * Desliga quando temperatura > setpoint + histerese.
 */
static void apply_control(void)
{
    if (!s_state.control_enabled) {
        relay_set(false);
        return;
    }

    float hyst  = CONFIG_HISTERESE_TEMP / 10.0f;
    float temp  = s_state.temperature;
    float sp    = (float)s_state.setpoint;

    if (temp < (sp - hyst)) {
        relay_set(true);
    } else if (temp > (sp + hyst)) {
        relay_set(false);
    }
    /* Dentro da histerese: mantém estado atual */
}

/* ------------------------------------------------------------------ */
/* Tarefa: leitura dos sensores                                        */
/* ------------------------------------------------------------------ */

static void task_sensors(void *pvParam)
{
    ESP_LOGI(TAG, "task_sensors iniciada");

    dht11_reading_t dht;
    ldr_reading_t   ldr;

    while (1) {
        /* DHT11 */
        if (dht11_read(&dht) == ESP_OK) {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_state.temperature = dht.temperature;
            s_state.humidity    = dht.humidity;
            xSemaphoreGive(s_state_mutex);
        } else {
            ESP_LOGW(TAG, "Falha na leitura do DHT11");
        }

        /* LDR */
        if (ldr_read(&ldr) == ESP_OK) {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_state.luminosity = ldr.percent;
            xSemaphoreGive(s_state_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_SENSOR_READ_INTERVAL_MS));
    }
}

/* ------------------------------------------------------------------ */
/* Tarefa: controle do relé                                            */
/* ------------------------------------------------------------------ */

static void task_control(void *pvParam)
{
    ESP_LOGI(TAG, "task_control iniciada");
    while (1) {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        apply_control();
        xSemaphoreGive(s_state_mutex);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ------------------------------------------------------------------ */
/* Tarefa: atualização do display OLED                                 */
/* ------------------------------------------------------------------ */

static void task_display(void *pvParam)
{
    ESP_LOGI(TAG, "task_display iniciada");
    while (1) {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        estufa_state_t snap = s_state; /* cópia local */
        xSemaphoreGive(s_state_mutex);

        oled_menu_update(&snap);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ------------------------------------------------------------------ */
/* Tarefa: registro de dados (logging)                                 */
/* ------------------------------------------------------------------ */

static void task_logging(void *pvParam)
{
    ESP_LOGI(TAG, "task_logging iniciada");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_LOG_INTERVAL_SEC * 1000));

        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        bool log_en = s_state.logging_enabled;
        estufa_state_t snap = s_state;
        xSemaphoreGive(s_state_mutex);

        if (log_en) {
            esp_err_t ret = data_logger_write(&snap);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Erro ao gravar log: %s", esp_err_to_name(ret));
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* ISR e callback dos botões                                           */
/* ------------------------------------------------------------------ */

static void IRAM_ATTR isr_btn_up(void *arg)
{
    /* Debounce simplificado via flag (melhorar com timer se necessário) */
    oled_menu_btn_up();
}

static void IRAM_ATTR isr_btn_sel(void *arg)
{
    oled_menu_btn_sel(&s_state);
}

static void buttons_init(void)
{
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BTN_UP_GPIO) | (1ULL << BTN_SEL_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_UP_GPIO,  isr_btn_up,  NULL);
    gpio_isr_handler_add(BTN_SEL_GPIO, isr_btn_sel, NULL);
    ESP_LOGI(TAG, "Botões configurados: UP=GPIO%d  SEL=GPIO%d",
             BTN_UP_GPIO, BTN_SEL_GPIO);
}

static void relay_init(void)
{
    gpio_config_t relay_cfg = {
        .pin_bit_mask = (1ULL << RELAY_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&relay_cfg);
    relay_set(false);
    ESP_LOGI(TAG, "Relé configurado no GPIO%d", RELAY_GPIO);
}

/* ------------------------------------------------------------------ */
/* Ponto de entrada da aplicação                                       */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "=== Sistema de Monitoramento de Estufa v1.0 ===");
    ESP_LOGI(TAG, "Franzininho WiFi LAB01 – ESP-IDF %s", esp_get_idf_version());

    /* Mutex do estado compartilhado */
    s_state_mutex = xSemaphoreCreateMutex();
    configASSERT(s_state_mutex);

    /* Inicialização dos componentes de hardware */
    ESP_ERROR_CHECK(dht11_init(CONFIG_DHT11_GPIO));
    ESP_ERROR_CHECK(ldr_init((adc1_channel_t)CONFIG_LDR_ADC_CHANNEL));
    ESP_ERROR_CHECK(oled_menu_init(CONFIG_OLED_I2C_SDA, CONFIG_OLED_I2C_SCL,
                                   CONFIG_OLED_I2C_ADDR));
    ESP_ERROR_CHECK(data_logger_init());
    ESP_ERROR_CHECK(serial_cmd_init(&s_state));

    relay_init();
    buttons_init();

    /* Delay para estabilização dos sensores */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Criação das tarefas FreeRTOS */
    xTaskCreatePinnedToCore(task_sensors, "sensors", 4096, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(task_control, "control", 2048, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_display, "display", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(task_logging, "logging", 4096, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "Sistema iniciado. Todas as tarefas ativas.");

    /* app_main retorna – o scheduler FreeRTOS assume o controle */
}

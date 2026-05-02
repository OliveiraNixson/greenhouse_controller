/**
 * @file dht11.c
 * @brief Implementação do driver DHT11 para ESP-IDF
 *
 * Protocolo DHT11:
 *  1. Host puxa linha LOW por ≥18 ms (sinal de início)
 *  2. Host libera linha e aguarda resposta do sensor (20-40 µs)
 *  3. Sensor responde: 80 µs LOW + 80 µs HIGH
 *  4. Transmissão de 40 bits: bit 0 = 26-28µs HIGH; bit 1 = 70µs HIGH
 *  5. Byte 1: umidade inteira | Byte 2: umidade decimal
 *     Byte 3: temp inteira    | Byte 4: temp decimal
 *     Byte 5: checksum (soma dos 4 bytes anteriores)
 */

#include "dht11.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DHT11";
static gpio_num_t s_gpio = -1;

/* ------------------------------------------------------------------ */
/* Helpers de temporização em µs (busy-wait)                          */
/* ------------------------------------------------------------------ */

static inline void delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

/**
 * @brief Aguarda que o pino atinja o nível esperado ou estoure timeout.
 * @param level   Nível esperado (0 ou 1)
 * @param timeout Timeout em µs
 * @return Tempo gasto em µs, ou -1 em timeout
 */
static int wait_for_level(int level, uint32_t timeout_us)
{
    uint32_t elapsed = 0;
    while (gpio_get_level(s_gpio) != level) {
        if (elapsed >= timeout_us) return -1;
        delay_us(1);
        elapsed++;
    }
    return (int)elapsed;
}

/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

esp_err_t dht11_init(gpio_num_t gpio_num)
{
    s_gpio = gpio_num;
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD, /* open-drain */
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar GPIO %d: %s", gpio_num, esp_err_to_name(ret));
        return ret;
    }
    gpio_set_level(s_gpio, 1); /* Linha em repouso HIGH */
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "DHT11 inicializado no GPIO %d", gpio_num);
    return ESP_OK;
}

esp_err_t dht11_read(dht11_reading_t *reading)
{
    if (!reading) return ESP_ERR_INVALID_ARG;

    reading->valid = false;
    uint8_t data[5] = {0};

    /* Desabilita interrupções para garantir timing preciso */
    portDISABLE_INTERRUPTS();

    /* --- Sinal de início: HOST puxa LOW por 18 ms --- */
    gpio_set_direction(s_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_gpio, 0);
    delay_us(18000);

    /* Libera linha e aguarda resposta do sensor */
    gpio_set_level(s_gpio, 1);
    delay_us(30);
    gpio_set_direction(s_gpio, GPIO_MODE_INPUT);

    /* --- Resposta do sensor: LOW 80µs + HIGH 80µs --- */
    if (wait_for_level(0, 100) < 0) { portENABLE_INTERRUPTS(); return ESP_ERR_TIMEOUT; }
    if (wait_for_level(1, 100) < 0) { portENABLE_INTERRUPTS(); return ESP_ERR_TIMEOUT; }
    if (wait_for_level(0, 100) < 0) { portENABLE_INTERRUPTS(); return ESP_ERR_TIMEOUT; }

    /* --- Leitura dos 40 bits de dados --- */
    for (int i = 0; i < 40; i++) {
        /* Cada bit começa com ~50 µs LOW */
        if (wait_for_level(1, 70) < 0) { portENABLE_INTERRUPTS(); return ESP_ERR_TIMEOUT; }
        /* Largura do pulso HIGH determina bit 0 (≤28µs) ou bit 1 (≥68µs) */
        int pulse = wait_for_level(0, 90);
        if (pulse < 0) { portENABLE_INTERRUPTS(); return ESP_ERR_TIMEOUT; }
        data[i / 8] <<= 1;
        if (pulse > 40) data[i / 8] |= 1; /* bit 1 */
    }

    portENABLE_INTERRUPTS();

    /* --- Verificação de checksum --- */
    uint8_t chk = data[0] + data[1] + data[2] + data[3];
    if (chk != data[4]) {
        ESP_LOGW(TAG, "Checksum inválido: calculado=0x%02X recebido=0x%02X", chk, data[4]);
        return ESP_ERR_INVALID_CRC;
    }

    reading->humidity    = data[0] + data[1] * 0.1f;
    reading->temperature = data[2] + data[3] * 0.1f;
    reading->valid       = true;

    ESP_LOGD(TAG, "Temp=%.1f°C  Umidade=%.1f%%", reading->temperature, reading->humidity);
    return ESP_OK;
}

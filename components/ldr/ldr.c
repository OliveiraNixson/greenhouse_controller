/**
 * @file ldr.c
 * @brief Implementação da leitura do LDR via ADC1 do ESP32
 *
 * Esquema de hardware:
 *   3.3V ──[ R_fixo 10kΩ ]──┬── ADC_PIN
 *                            │
 *                          [LDR]
 *                            │
 *                           GND
 *
 * Quanto mais luz → LDR tem menor resistência → tensão no pino cai
 * → valor ADC menor → mapeamos para luminosidade MAIOR (inversão).
 *
 * Atenuação: ADC_ATTEN_DB_11 permite ler até ~3.1V (cobre 0–3.3V).
 */

#include "ldr.h"
#include "esp_log.h"
#include "esp_adc_cal.h"

#define NUM_SAMPLES     16      /* Número de amostras para média */
#define ADC_WIDTH       ADC_WIDTH_BIT_12
#define ADC_ATTEN       ADC_ATTEN_DB_11
#define ADC_MAX_RAW     4095

static const char *TAG = "LDR";
static adc1_channel_t s_channel;
static esp_adc_cal_characteristics_t s_adc_chars;

esp_err_t ldr_init(adc1_channel_t channel)
{
    s_channel = channel;

    /* Configura ADC1 */
    esp_err_t ret = adc1_config_width(ADC_WIDTH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao configurar largura ADC: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = adc1_config_channel_atten(channel, ADC_ATTEN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao configurar atenuação ADC: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Calibração do ADC */
    esp_adc_cal_value_t cal_type = esp_adc_cal_characterize(
        ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &s_adc_chars
    );

    if (cal_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "Calibração ADC: eFuse Vref");
    } else if (cal_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "Calibração ADC: Two Point eFuse");
    } else {
        ESP_LOGW(TAG, "Calibração ADC: Vref padrão (menos preciso)");
    }

    ESP_LOGI(TAG, "LDR inicializado no canal ADC1 %d", channel);
    return ESP_OK;
}

esp_err_t ldr_read(ldr_reading_t *reading)
{
    if (!reading) return ESP_ERR_INVALID_ARG;

    /* Média de NUM_SAMPLES amostras */
    uint32_t sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += adc1_get_raw(s_channel);
    }
    uint32_t raw = sum / NUM_SAMPLES;

    reading->raw   = (uint16_t)raw;
    /* Inversão: ADC baixo = muita luz */
    reading->percent = (uint8_t)((ADC_MAX_RAW - raw) * 100UL / ADC_MAX_RAW);
    reading->valid   = true;

    ESP_LOGD(TAG, "LDR raw=%u  luminosidade=%u%%", reading->raw, reading->percent);
    return ESP_OK;
}

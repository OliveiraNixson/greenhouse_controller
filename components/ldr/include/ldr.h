#pragma once

/**
 * @file ldr.h
 * @brief Componente para leitura de luminosidade via LDR e ADC
 *
 * O LDR é utilizado em divisor de tensão com resistor fixo.
 * A tensão resultante é lida pelo ADC1 do ESP32 e convertida
 * em percentual de luminosidade (0–100%).
 */

#include <stdint.h>
#include "esp_err.h"
#include "driver/adc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Resultado de uma leitura de luminosidade */
typedef struct {
    uint16_t raw;        /**< Valor bruto do ADC (0–4095) */
    uint8_t  percent;    /**< Luminosidade percentual (0=escuro, 100=plena luz) */
    bool     valid;
} ldr_reading_t;

/**
 * @brief Inicializa o ADC para leitura do LDR.
 * @param channel  Canal ADC1 (adc1_channel_t) onde o LDR está conectado.
 * @return ESP_OK em sucesso.
 */
esp_err_t ldr_init(adc1_channel_t channel);

/**
 * @brief Lê o valor de luminosidade do LDR.
 *
 * Realiza múltiplas amostras e retorna a média para reduzir ruído.
 *
 * @param[out] reading  Ponteiro para estrutura de resultado.
 * @return ESP_OK em sucesso.
 */
esp_err_t ldr_read(ldr_reading_t *reading);

#ifdef __cplusplus
}
#endif

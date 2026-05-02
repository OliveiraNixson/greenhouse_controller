#pragma once

/**
 * @file dht11.h
 * @brief Componente para leitura do sensor DHT11 (temperatura e umidade)
 *
 * Implementa o protocolo single-wire do DHT11 via GPIO.
 * Compatível com ESP-IDF v5.x utilizando esp_timer e GPIO driver.
 */

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Resultado de uma leitura do DHT11 */
typedef struct {
    float temperature;   /**< Temperatura em graus Celsius */
    float humidity;      /**< Umidade relativa em % */
    bool  valid;         /**< true se a leitura foi bem-sucedida */
} dht11_reading_t;

/**
 * @brief Inicializa o pino GPIO para comunicação com o DHT11.
 * @param gpio_num  Número do GPIO conectado ao DATA do DHT11.
 * @return ESP_OK em sucesso.
 */
esp_err_t dht11_init(gpio_num_t gpio_num);

/**
 * @brief Realiza a leitura de temperatura e umidade do DHT11.
 *
 * Bloqueia por ~20 ms durante a aquisição. Deve ser chamada
 * com intervalo mínimo de 1 segundo entre leituras.
 *
 * @param[out] reading  Ponteiro para estrutura que receberá os dados.
 * @return ESP_OK em sucesso, ESP_ERR_TIMEOUT ou ESP_ERR_INVALID_CRC em falha.
 */
esp_err_t dht11_read(dht11_reading_t *reading);

#ifdef __cplusplus
}
#endif

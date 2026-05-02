#pragma once

/**
 * @file data_logger.h
 * @brief Componente de registro de dados em SPIFFS/LittleFS
 *
 * Salva leituras dos sensores em arquivo texto (/spiffs/log.txt).
 * Cada linha do log segue o formato CSV:
 *   timestamp_ms,temperatura,umidade,luminosidade,setpoint,relay
 *
 * Funcionalidades:
 *  - Inicialização e montagem do sistema de arquivos
 *  - Gravação periódica de entradas
 *  - Rotação automática ao atingir LOG_MAX_LINES
 *  - Dump completo via UART para debugging
 *  - Apagar o log via comando serial
 */

#include <stdbool.h>
#include "esp_err.h"
#include "oled_menu.h"   /* para estufa_state_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o sistema de arquivos SPIFFS e o módulo de logging.
 * @return ESP_OK em sucesso.
 */
esp_err_t data_logger_init(void);

/**
 * @brief Grava uma entrada no arquivo de log.
 *
 * Formato: "timestamp,temp,umidade,luz,setpoint,relay\n"
 *
 * @param state  Estado atual da estufa a ser registrado.
 * @return ESP_OK em sucesso, ESP_FAIL se logging desabilitado ou erro de IO.
 */
esp_err_t data_logger_write(const estufa_state_t *state);

/**
 * @brief Imprime todo o conteúdo do arquivo de log na porta serial (UART0).
 */
void data_logger_dump(void);

/**
 * @brief Apaga o arquivo de log.
 * @return ESP_OK em sucesso.
 */
esp_err_t data_logger_clear(void);

/**
 * @brief Retorna o número de linhas atualmente no arquivo de log.
 */
uint32_t data_logger_line_count(void);

#ifdef __cplusplus
}
#endif

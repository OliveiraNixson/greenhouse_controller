#pragma once

/**
 * @file serial_cmd.h
 * @brief Componente de interface de comandos via UART (serial)
 *
 * Interpreta comandos recebidos pela UART0 (monitor serial).
 *
 * Comandos disponíveis:
 *   help          - Lista todos os comandos
 *   dump          - Exibe o conteúdo do arquivo de log
 *   clear         - Apaga o arquivo de log
 *   status        - Exibe o estado atual da estufa
 *   log on|off    - Habilita/desabilita o registro de dados
 *   ctrl on|off   - Liga/desliga o controle de temperatura
 *   sp <valor>    - Define o set point de temperatura
 */

#include "esp_err.h"
#include "oled_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o módulo de comandos seriais.
 *
 * Cria uma tarefa FreeRTOS que monitora a UART0
 * e processa comandos recebidos.
 *
 * @param state  Ponteiro para o estado compartilhado da estufa.
 * @return ESP_OK em sucesso.
 */
esp_err_t serial_cmd_init(estufa_state_t *state);

#ifdef __cplusplus
}
#endif

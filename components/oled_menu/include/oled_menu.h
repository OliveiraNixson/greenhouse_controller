#pragma once

/**
 * @file oled_menu.h
 * @brief Componente para controle do display OLED SSD1306 e menu interativo
 *
 * Utiliza a biblioteca ssd1306 do ESP-IDF Component Registry.
 * O menu é navegado via dois botões: BTN_UP e BTN_SEL (seleção/enter).
 *
 * Telas disponíveis:
 *  - SCREEN_MONITOR : exibição em tempo real dos sensores
 *  - SCREEN_MENU    : menu principal
 *  - SCREEN_SETPOINT: ajuste do set point de temperatura
 *  - SCREEN_CONFIG  : configurações (logging, controle on/off)
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Telas do sistema */
typedef enum {
    SCREEN_MONITOR = 0,
    SCREEN_MENU,
    SCREEN_SETPOINT,
    SCREEN_CONFIG,
} screen_t;

/** Estado compartilhado entre tarefas */
typedef struct {
    float    temperature;       /**< Temperatura lida (°C) */
    float    humidity;          /**< Umidade lida (%) */
    uint8_t  luminosity;        /**< Luminosidade (0-100%) */
    int      setpoint;          /**< Set point de temperatura (°C) */
    bool     control_enabled;   /**< Sistema de controle ligado? */
    bool     logging_enabled;   /**< Logging habilitado? */
    bool     relay_on;          /**< Estado atual do relé */
} estufa_state_t;

/**
 * @brief Inicializa o display OLED via I2C.
 * @param sda   GPIO SDA
 * @param scl   GPIO SCL
 * @param addr  Endereço I2C do display
 * @return ESP_OK em sucesso
 */
esp_err_t oled_menu_init(int sda, int scl, uint8_t addr);

/**
 * @brief Atualiza o display com o estado atual do sistema.
 *
 * Deve ser chamada periodicamente (ex: a cada 500 ms).
 * A tela exibida depende do estado interno do menu.
 *
 * @param state  Ponteiro para o estado atual da estufa (somente leitura)
 */
void oled_menu_update(const estufa_state_t *state);

/**
 * @brief Processa evento do botão UP (navegar/incrementar).
 */
void oled_menu_btn_up(void);

/**
 * @brief Processa evento do botão SELECT/ENTER (confirmar/avançar).
 * @param[out] state  Estado modificado pelo menu (ex: novo setpoint)
 */
void oled_menu_btn_sel(estufa_state_t *state);

/**
 * @brief Retorna a tela atualmente ativa.
 */
screen_t oled_menu_get_screen(void);

#ifdef __cplusplus
}
#endif

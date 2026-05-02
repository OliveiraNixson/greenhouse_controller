/**
 * @file oled_menu.c
 * @brief Implementação do menu OLED para o sistema de estufa
 *
 * Utiliza a biblioteca ssd1306 (https://components.espressif.com/components/espressif/ssd1306)
 * para comunicação com o display 128×64 via I2C.
 *
 * Estrutura do menu:
 *
 *   [MONITOR] ─── BTN_SEL ───► [MENU PRINCIPAL]
 *                                   ├── "Ajustar Temp" ──► [SETPOINT]
 *                                   ├── "Configurações" ─► [CONFIG]
 *                                   └── "Voltar"        ──► [MONITOR]
 */

#include "oled_menu.h"
#include "ssd1306.h"
#include "font8x8_basic.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "OLED_MENU";

/* Handle do display */
static SSD1306_t s_dev;

/* Estado interno do menu */
static screen_t  s_screen      = SCREEN_MONITOR;
static int       s_menu_sel    = 0;   /* item selecionado no menu */
static int       s_setpoint_edit = 0; /* valor temporário durante edição */

#define MENU_ITEMS 3
static const char *s_menu_labels[MENU_ITEMS] = {
    "Ajustar Temp",
    "Configuracoes",
    "Voltar",
};

/* ------------------------------------------------------------------ */
/* Helpers de desenho                                                  */
/* ------------------------------------------------------------------ */

static void oled_clear(void)
{
    ssd1306_clear_screen(&s_dev, false);
}

static void oled_text(int col, int row, const char *text, bool invert)
{
    ssd1306_display_text(&s_dev, row, (char *)text, strlen(text), invert);
    (void)col; /* biblioteca gerencia colunas via string completa */
}

static void oled_show(void)
{
    /* No driver ssd1306, o display atualiza automaticamente a cada chamada
       de ssd1306_display_text. Função reservada para futuras extensões. */
}

/* ------------------------------------------------------------------ */
/* Telas                                                               */
/* ------------------------------------------------------------------ */

static void draw_monitor(const estufa_state_t *s)
{
    char buf[22];
    oled_clear();

    /* Linha 0: título */
    oled_text(0, 0, "=== ESTUFA ===  ", false);

    /* Linha 2: temperatura */
    snprintf(buf, sizeof(buf), "Temp:  %5.1f C    ", s->temperature);
    oled_text(0, 2, buf, false);

    /* Linha 3: umidade */
    snprintf(buf, sizeof(buf), "Umid:  %5.1f %%   ", s->humidity);
    oled_text(0, 3, buf, false);

    /* Linha 4: luminosidade */
    snprintf(buf, sizeof(buf), "Luz:   %5u %%   ", s->luminosity);
    oled_text(0, 4, buf, false);

    /* Linha 5: set point */
    snprintf(buf, sizeof(buf), "SP:  %3d C  %s   ",
             s->setpoint, s->relay_on ? "[ON]" : "[--]");
    oled_text(0, 5, buf, false);

    /* Linha 7: status */
    snprintf(buf, sizeof(buf), "%s %s",
             s->control_enabled ? "CTRL:ON" : "CTRL:OFF",
             s->logging_enabled ? "LOG:ON" : "LOG:OFF");
    oled_text(0, 7, buf, false);
}

static void draw_menu(void)
{
    char buf[22];
    oled_clear();
    oled_text(0, 0, "=== MENU ===    ", false);

    for (int i = 0; i < MENU_ITEMS; i++) {
        bool sel = (i == s_menu_sel);
        snprintf(buf, sizeof(buf), "%s%-16s",
                 sel ? "> " : "  ", s_menu_labels[i]);
        oled_text(0, 2 + i, buf, sel);
    }
}

static void draw_setpoint(int sp)
{
    char buf[22];
    oled_clear();
    oled_text(0, 0, "Ajustar Setpoint", false);
    oled_text(0, 2, "  UP: incrementa", false);
    oled_text(0, 3, "  SEL: confirmar", false);
    snprintf(buf, sizeof(buf), "  Temp: %3d C   ", sp);
    oled_text(0, 5, buf, true); /* invertido para destaque */
}

static void draw_config(const estufa_state_t *s)
{
    char buf[22];
    oled_clear();
    oled_text(0, 0, "=== CONFIG ===  ", false);
    snprintf(buf, sizeof(buf), "> Controle: %s   ", s->control_enabled ? "ON " : "OFF");
    oled_text(0, 2, buf, s_menu_sel == 0);
    snprintf(buf, sizeof(buf), "> Logging:  %s   ", s->logging_enabled ? "ON " : "OFF");
    oled_text(0, 3, buf, s_menu_sel == 1);
    oled_text(0, 5, "> Voltar        ", s_menu_sel == 2);
}

/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

esp_err_t oled_menu_init(int sda, int scl, uint8_t addr)
{
#if CONFIG_IDF_TARGET_ESP32
    i2c_master_init(&s_dev, sda, scl, -1);
#endif
    ssd1306_init(&s_dev, 128, 64);
    ssd1306_clear_screen(&s_dev, false);
    ssd1306_contrast(&s_dev, 0xFF);
    ssd1306_display_text(&s_dev, 0, "  Estufa v1.0   ", 16, false);
    ssd1306_display_text(&s_dev, 2, " Inicializando..", 16, false);
    ESP_LOGI(TAG, "OLED inicializado (SDA=%d SCL=%d ADDR=0x%02X)", sda, scl, addr);
    return ESP_OK;
}

void oled_menu_update(const estufa_state_t *state)
{
    switch (s_screen) {
        case SCREEN_MONITOR:  draw_monitor(state);            break;
        case SCREEN_MENU:     draw_menu();                    break;
        case SCREEN_SETPOINT: draw_setpoint(s_setpoint_edit); break;
        case SCREEN_CONFIG:   draw_config(state);             break;
    }
    oled_show();
}

void oled_menu_btn_up(void)
{
    switch (s_screen) {
        case SCREEN_MENU:
            s_menu_sel = (s_menu_sel + 1) % MENU_ITEMS;
            break;
        case SCREEN_SETPOINT:
            if (s_setpoint_edit < CONFIG_SETPOINT_TEMP_MAX)
                s_setpoint_edit++;
            break;
        case SCREEN_CONFIG:
            s_menu_sel = (s_menu_sel + 1) % 3;
            break;
        default:
            break;
    }
}

void oled_menu_btn_sel(estufa_state_t *state)
{
    switch (s_screen) {
        case SCREEN_MONITOR:
            /* Entra no menu principal */
            s_menu_sel = 0;
            s_screen   = SCREEN_MENU;
            break;

        case SCREEN_MENU:
            switch (s_menu_sel) {
                case 0: /* Ajustar Temp */
                    s_setpoint_edit = state->setpoint;
                    s_screen = SCREEN_SETPOINT;
                    break;
                case 1: /* Configurações */
                    s_menu_sel = 0;
                    s_screen   = SCREEN_CONFIG;
                    break;
                case 2: /* Voltar */
                    s_screen = SCREEN_MONITOR;
                    break;
            }
            break;

        case SCREEN_SETPOINT:
            /* Confirma o novo set point */
            state->setpoint = s_setpoint_edit;
            ESP_LOGI(TAG, "Novo setpoint: %d°C", state->setpoint);
            s_screen = SCREEN_MENU;
            break;

        case SCREEN_CONFIG:
            switch (s_menu_sel) {
                case 0:
                    state->control_enabled = !state->control_enabled;
                    ESP_LOGI(TAG, "Controle: %s", state->control_enabled ? "ON" : "OFF");
                    break;
                case 1:
                    state->logging_enabled = !state->logging_enabled;
                    ESP_LOGI(TAG, "Logging: %s", state->logging_enabled ? "ON" : "OFF");
                    break;
                case 2:
                    s_screen = SCREEN_MENU;
                    break;
            }
            break;
    }
}

screen_t oled_menu_get_screen(void)
{
    return s_screen;
}

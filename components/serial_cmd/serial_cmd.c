/**
 * @file serial_cmd.c
 * @brief Processamento de comandos seriais via UART0
 *
 * A tarefa serial_task() aguarda caracteres da UART0,
 * acumula em um buffer de linha e, ao receber '\n' ou '\r',
 * despacha o comando para o handler correspondente.
 */

#include "serial_cmd.h"
#include "data_logger.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "SERIAL_CMD";

#define UART_NUM      UART_NUM_0
#define BUF_SIZE      256
#define TASK_STACK    4096
#define TASK_PRIORITY 5

static estufa_state_t *s_state = NULL;

/* ------------------------------------------------------------------ */
/* Utilitários                                                         */
/* ------------------------------------------------------------------ */

static void trim(char *s)
{
    /* Remove espaços e \r\n do início e fim */
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    memmove(s, p, strlen(p) + 1);
    int len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Handlers de comandos                                                */
/* ------------------------------------------------------------------ */

static void cmd_help(void)
{
    printf("\r\n=== Comandos disponíveis ===\r\n");
    printf("  help          - Lista de comandos\r\n");
    printf("  status        - Estado atual da estufa\r\n");
    printf("  dump          - Exibe o arquivo de log\r\n");
    printf("  clear         - Apaga o arquivo de log\r\n");
    printf("  log on|off    - Habilita/desabilita logging\r\n");
    printf("  ctrl on|off   - Liga/desliga controle de temperatura\r\n");
    printf("  sp <valor>    - Define set point de temperatura (°C)\r\n");
    printf("============================\r\n\r\n");
}

static void cmd_status(void)
{
    if (!s_state) return;
    printf("\r\n=== STATUS DA ESTUFA ===\r\n");
    printf("  Temperatura : %.1f °C\r\n", s_state->temperature);
    printf("  Umidade     : %.1f %%\r\n", s_state->humidity);
    printf("  Luminosidade: %u %%\r\n",   s_state->luminosity);
    printf("  Set Point   : %d °C\r\n",   s_state->setpoint);
    printf("  Relé        : %s\r\n",       s_state->relay_on      ? "LIGADO"    : "DESLIGADO");
    printf("  Controle    : %s\r\n",       s_state->control_enabled ? "ATIVO"   : "INATIVO");
    printf("  Logging     : %s (%u linhas)\r\n",
           s_state->logging_enabled ? "ATIVO" : "INATIVO",
           data_logger_line_count());
    printf("========================\r\n\r\n");
}

static void process_command(char *line)
{
    trim(line);
    if (strlen(line) == 0) return;

    printf("\r\n> %s\r\n", line);

    /* Tokenização */
    char *cmd  = strtok(line, " ");
    char *arg1 = strtok(NULL, " ");

    if (!cmd) return;

    /* Converte comando para minúsculas */
    for (char *p = cmd; *p; p++) *p = tolower((unsigned char)*p);

    if (strcmp(cmd, "help") == 0) {
        cmd_help();

    } else if (strcmp(cmd, "status") == 0) {
        cmd_status();

    } else if (strcmp(cmd, "dump") == 0) {
        data_logger_dump();

    } else if (strcmp(cmd, "clear") == 0) {
        data_logger_clear();
        printf("Log apagado.\r\n\r\n");

    } else if (strcmp(cmd, "log") == 0 && arg1) {
        if (strcmp(arg1, "on") == 0) {
            s_state->logging_enabled = true;
            printf("Logging HABILITADO.\r\n\r\n");
        } else if (strcmp(arg1, "off") == 0) {
            s_state->logging_enabled = false;
            printf("Logging DESABILITADO.\r\n\r\n");
        } else {
            printf("Uso: log on|off\r\n\r\n");
        }

    } else if (strcmp(cmd, "ctrl") == 0 && arg1) {
        if (strcmp(arg1, "on") == 0) {
            s_state->control_enabled = true;
            printf("Controle HABILITADO.\r\n\r\n");
        } else if (strcmp(arg1, "off") == 0) {
            s_state->control_enabled = false;
            printf("Controle DESABILITADO.\r\n\r\n");
        } else {
            printf("Uso: ctrl on|off\r\n\r\n");
        }

    } else if (strcmp(cmd, "sp") == 0 && arg1) {
        int val = atoi(arg1);
        if (val >= CONFIG_SETPOINT_TEMP_MIN && val <= CONFIG_SETPOINT_TEMP_MAX) {
            s_state->setpoint = val;
            printf("Set point definido para %d °C.\r\n\r\n", val);
        } else {
            printf("Valor inválido. Faixa: %d – %d °C\r\n\r\n",
                   CONFIG_SETPOINT_TEMP_MIN, CONFIG_SETPOINT_TEMP_MAX);
        }

    } else {
        printf("Comando desconhecido: '%s'. Digite 'help'.\r\n\r\n", cmd);
    }
}

/* ------------------------------------------------------------------ */
/* Tarefa principal                                                    */
/* ------------------------------------------------------------------ */

static void serial_task(void *pvParam)
{
    char     buf[BUF_SIZE];
    uint8_t  rx;
    int      pos = 0;

    printf("\r\n=== Estufa Franzininho WiFi v1.0 ===\r\n");
    printf("Digite 'help' para listar os comandos.\r\n\r\n");

    while (1) {
        int n = uart_read_bytes(UART_NUM, &rx, 1, pdMS_TO_TICKS(50));
        if (n <= 0) continue;

        /* Eco do caractere recebido */
        uart_write_bytes(UART_NUM, (const char *)&rx, 1);

        if (rx == '\r' || rx == '\n') {
            buf[pos] = '\0';
            if (pos > 0) {
                process_command(buf);
                pos = 0;
            }
        } else if (rx == 0x7F || rx == '\b') {
            /* Backspace */
            if (pos > 0) {
                pos--;
                printf(" \b"); /* apaga caractere no terminal */
            }
        } else {
            if (pos < BUF_SIZE - 1) {
                buf[pos++] = (char)rx;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

esp_err_t serial_cmd_init(estufa_state_t *state)
{
    s_state = state;

    /* A UART0 já está configurada pelo ESP-IDF para o console.
       Reconfiguramos apenas o buffer de RX para uso direto. */
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);

    BaseType_t rc = xTaskCreate(serial_task, "serial_cmd", TASK_STACK,
                                NULL, TASK_PRIORITY, NULL);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar tarefa serial_cmd");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Módulo de comandos seriais inicializado");
    return ESP_OK;
}

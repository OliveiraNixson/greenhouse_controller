/**
 * @file data_logger.c
 * @brief Implementação do registro de dados em SPIFFS
 *
 * O ESP32 da Franzininho WiFi LAB01 possui flash SPI de 4 MB.
 * A partição SPIFFS configurada ocupa 1 MB (suficiente para
 * milhares de linhas de log CSV).
 *
 * Rotação de log:
 *   Quando o arquivo ultrapassa CONFIG_LOG_MAX_LINES entradas,
 *   o arquivo é renomeado para log_old.txt e um novo log.txt é criado.
 */

#include "data_logger.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG        = "DATA_LOGGER";
static const char *LOG_PATH   = CONFIG_LOG_FILENAME;
static const char *LOG_OLD    = "/spiffs/log_old.txt";
static uint32_t   s_line_cnt  = 0;

/* ------------------------------------------------------------------ */
/* Helpers internos                                                    */
/* ------------------------------------------------------------------ */

/** Conta linhas do arquivo de log */
static uint32_t count_lines(void)
{
    FILE *f = fopen(LOG_PATH, "r");
    if (!f) return 0;
    uint32_t n = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') n++;
    }
    fclose(f);
    return n;
}

/** Remove arquivo se existir */
static void remove_if_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        remove(path);
    }
}

/** Rotaciona o arquivo de log quando muito grande */
static void rotate_if_needed(void)
{
    if (s_line_cnt < (uint32_t)CONFIG_LOG_MAX_LINES) return;

    ESP_LOGW(TAG, "Limite de %d linhas atingido. Rotacionando log...",
             CONFIG_LOG_MAX_LINES);
    remove_if_exists(LOG_OLD);
    rename(LOG_PATH, LOG_OLD);
    s_line_cnt = 0;
    ESP_LOGI(TAG, "Log rotacionado. Antigo salvo em %s", LOG_OLD);
}

/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

esp_err_t data_logger_init(void)
{
    /* Configuração e montagem do SPIFFS */
    esp_vfs_spiffs_conf_t conf = {
        .base_path              = "/spiffs",
        .partition_label        = NULL,  /* usa partição padrão */
        .max_files              = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Falha ao montar SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Partição SPIFFS não encontrada");
        } else {
            ESP_LOGE(TAG, "Erro ao inicializar SPIFFS: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    /* Informações de uso */
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS montado. Total: %u KB  Usado: %u KB",
             (unsigned)(total / 1024), (unsigned)(used / 1024));

    /* Cabeçalho do log (cria arquivo se não existir) */
    FILE *f = fopen(LOG_PATH, "r");
    if (!f) {
        f = fopen(LOG_PATH, "w");
        if (f) {
            fprintf(f, "timestamp_ms,temperatura,umidade,luminosidade,setpoint,relay\n");
            fclose(f);
            s_line_cnt = 1;
            ESP_LOGI(TAG, "Arquivo de log criado: %s", LOG_PATH);
        } else {
            ESP_LOGE(TAG, "Não foi possível criar %s", LOG_PATH);
            return ESP_FAIL;
        }
    } else {
        fclose(f);
        s_line_cnt = count_lines();
        ESP_LOGI(TAG, "Log existente: %u linhas", s_line_cnt);
    }

    return ESP_OK;
}

esp_err_t data_logger_write(const estufa_state_t *state)
{
    if (!state) return ESP_ERR_INVALID_ARG;

    rotate_if_needed();

    FILE *f = fopen(LOG_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "Erro ao abrir %s para escrita", LOG_PATH);
        return ESP_FAIL;
    }

    int64_t ts = esp_timer_get_time() / 1000; /* milissegundos */
    fprintf(f, "%lld,%.1f,%.1f,%u,%d,%d\n",
            ts,
            state->temperature,
            state->humidity,
            state->luminosity,
            state->setpoint,
            state->relay_on ? 1 : 0);

    fclose(f);
    s_line_cnt++;

    ESP_LOGD(TAG, "Log gravado (linha %u)", s_line_cnt);
    return ESP_OK;
}

void data_logger_dump(void)
{
    printf("\r\n=== DUMP DO LOG: %s ===\r\n", LOG_PATH);

    FILE *f = fopen(LOG_PATH, "r");
    if (!f) {
        printf("(arquivo não encontrado)\r\n");
        return;
    }

    char line[128];
    uint32_t n = 0;
    while (fgets(line, sizeof(line), f)) {
        /* Remove \n final para impressão limpa */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        printf("%s\r\n", line);
        n++;
        /* Yield para evitar watchdog em arquivos grandes */
        if (n % 50 == 0) vTaskDelay(pdMS_TO_TICKS(10));
    }
    fclose(f);
    printf("=== FIM DO LOG (%u linhas) ===\r\n\r\n", n);
}

esp_err_t data_logger_clear(void)
{
    remove(LOG_PATH);
    FILE *f = fopen(LOG_PATH, "w");
    if (!f) return ESP_FAIL;
    fprintf(f, "timestamp_ms,temperatura,umidade,luminosidade,setpoint,relay\n");
    fclose(f);
    s_line_cnt = 1;
    ESP_LOGI(TAG, "Log apagado.");
    return ESP_OK;
}

uint32_t data_logger_line_count(void)
{
    return s_line_cnt;
}

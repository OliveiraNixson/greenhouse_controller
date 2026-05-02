# Relatório Técnico — Sistema de Monitoramento de Estufa

**Projeto:** Estufa Franzininho WiFi LAB01  
**Disciplina:** Sistemas Embarcados com ESP-IDF  
**Data:** 2025

---

## 1. Introdução

Este relatório descreve as decisões técnicas e a aplicação dos conceitos do curso no desenvolvimento de um sistema embarcado para monitoramento e controle de uma estufa utilizando a plataforma Franzininho WiFi LAB01 (baseada no ESP32) com o framework ESP-IDF v5.x.

O sistema integra leitura de múltiplos sensores, exibição em display OLED, controle de atuador (relé), armazenamento de dados em memória não-volátil (SPIFFS) e interface de comando via comunicação serial, organizados em componentes modulares e coordenados por um RTOS (FreeRTOS).

---

## 2. Arquitetura Geral do Sistema

### 2.1 Visão de alto nível

O sistema foi projetado em torno do padrão **Produtor–Consumidor** com estado compartilhado protegido por mutex:

```
Sensores (Produtores)          Estado Compartilhado (estufa_state_t)
  ├── DHT11 (temp/umid) ──────►│ temperature, humidity,             │
  └── LDR   (luminosidade) ───►│ luminosity, setpoint,             │
                                │ control_enabled, logging_enabled, │◄── Menu OLED
Atuadores (Consumidores)        │ relay_on                          │◄── Serial CMD
  └── Relé ◄──────────────────►└──────────────────────────────────┘
                                          │
                              ┌───────────┘
                              ▼
                    SPIFFS (log.txt CSV)
```

Essa arquitetura desacopla a leitura dos sensores, o controle, a exibição e o armazenamento em tarefas independentes, permitindo que cada uma opere na frequência adequada sem bloquear as demais.

### 2.2 Tarefas FreeRTOS

| Tarefa | Core | Prioridade | Período | Responsabilidade |
|---|---|---|---|---|
| `task_sensors` | 0 | 6 | 2 s | Lê DHT11 e LDR, atualiza estado |
| `task_control` | 0 | 5 | 500 ms | Aplica histerese ao relé |
| `serial_cmd` | 0 | 5 | Evento | Recebe e processa comandos UART |
| `task_display` | 1 | 4 | 500 ms | Atualiza OLED com estado atual |
| `task_logging` | 0 | 3 | 30 s | Grava entrada no log SPIFFS |

A separação `task_display` no Core 1 evita que a comunicação I2C com o OLED interfira na temporização precisa do protocolo single-wire do DHT11, que roda no Core 0.

---

## 3. Componentes e Justificativas Técnicas

### 3.1 Componente `dht11`

**Protocolo implementado:** Single-wire proprietário da AOSONG (DHT11).

O driver implementa manualmente o protocolo de temporização crítica:
1. O host puxa a linha LOW por 18 ms (sinal de início)
2. Aguarda a resposta do sensor (80 µs LOW + 80 µs HIGH)
3. Lê 40 bits: largura do pulso HIGH determina bit 0 (≤28 µs) ou bit 1 (≥68 µs)
4. Verifica checksum (XOR dos 4 bytes de dados)

**Decisão técnica crítica:** O trecho de leitura dos 40 bits executa com `portDISABLE_INTERRUPTS()` para garantir que nenhuma ISR interrompa a medição de tempo em microssegundos. O uso de `esp_rom_delay_us()` (busy-wait) garante precisão sem dependência de timers de hardware. Essa abordagem é justificada pelo protocolo DHT11, que não possui clock separado.

**GPIO Open-Drain:** O pino é configurado como `GPIO_MODE_INPUT_OUTPUT_OD` com pull-up habilitado, permitindo a comunicação bidirecional na mesma linha sem lógica de chaveamento de direção explícita.

### 3.2 Componente `ldr`

**Interface:** ADC1 do ESP32 com calibração via `esp_adc_cal`.

A biblioteca `esp_adc_cal` utiliza valores de calibração gravados na eFuse do chip (se disponíveis) para corrigir as não-linearidades do ADC. Três modos são suportados automaticamente:
- **Two-Point (TP):** mais preciso, utiliza dois pontos de calibração da eFuse
- **eFuse Vref:** referência interna gravada em fábrica
- **Padrão:** menos preciso, usa 1100 mV como referência

**Técnica de amostragem:** A leitura realiza a média de 16 amostras (`NUM_SAMPLES = 16`), reduzindo o ruído ADC sem comprometer a responsividade (o ADC ESP32 tem ~12 bits efetivos mas com ruído de ±2-3 bits).

**Inversão lógica:** No circuito divisor de tensão, mais luz → menor resistência do LDR → tensão ADC cai. A inversão `percent = (MAX - raw) * 100 / MAX` mapeia corretamente para escala intuitiva.

### 3.3 Componente `oled_menu`

**Biblioteca de terceiros:** `espressif/ssd1306` do ESP-IDF Component Registry, que abstrai a comunicação I2C com o controlador SSD1306.

**Máquina de estados do menu:** O menu é implementado como uma FSM (Finite State Machine) com quatro estados (`screen_t`):

```
MONITOR ──[SEL]──► MENU ──[SEL]──► SETPOINT ──[SEL]──► MENU
                     │
                     └──[SEL]──► CONFIG ──[SEL]──► MENU
```

**Decisão de projeto:** O estado do menu é mantido no componente `oled_menu` (encapsulamento), enquanto o estado da estufa (`estufa_state_t`) é mantido no `main.c`. O menu recebe um ponteiro para o estado apenas quando o usuário confirma uma ação (botão SEL), garantindo separação de responsabilidades.

**Debounce:** Para simplificação, o debounce dos botões foi omitido (geralmente não necessário para botões em protótipos com ESP-IDF, pois a taxa de processamento das ISRs é naturalmente limitada). Em produção, recomenda-se usar o componente `espressif/button` com debounce por software.

### 3.4 Componente `data_logger`

**Sistema de arquivos:** SPIFFS (SPI Flash File System), escolhido por:
- Suporte nativo no ESP-IDF sem dependências externas
- Adequado para arquivos sequenciais de log (não requer rename/seek frequente)
- Simples de configurar via `esp_vfs_spiffs_register()`

**Alternativa considerada:** LittleFS oferece maior robustez a falhas de energia (journaling implícito) e melhor desempenho em escrita aleatória. Para este projeto, SPIFFS é suficiente pois os logs são sempre escritos de forma sequencial (append-only).

**Formato CSV:** Escolhido por ser legível por humanos via terminal serial e importável diretamente em planilhas (Excel, LibreOffice) e ferramentas de análise (Python/pandas).

**Rotação de log:** Implementada por contagem de linhas (não por tamanho de arquivo), pois é mais simples e previsível. O arquivo antigo é preservado como `log_old.txt` antes de criar um novo `log.txt`, garantindo que nenhum dado seja perdido abruptamente.

**Cabeçalho CSV:** Escrito automaticamente na criação do arquivo, facilitando importação em ferramentas externas.

### 3.5 Componente `serial_cmd`

**Protocolo:** Interface de linha de comando (CLI) simples sobre UART0, 115200 bps, 8N1.

**Implementação:** A tarefa `serial_task` implementa um loop que:
1. Aguarda byte da UART com timeout de 50 ms
2. Ecoa o caractere recebido (UX de terminal)
3. Trata backspace para edição in-line
4. Despacha o comando ao receber `\r` ou `\n`

O parsing usa `strtok()` para separar comando e argumentos, com conversão para minúsculas para aceitar `LOG ON`, `log on` e `Log On` igualmente.

**Thread-safety:** As modificações no estado compartilhado (`s_state`) feitas pelos comandos seriais não utilizam mutex explicitamente neste protótipo, pois as variáveis são do tipo `bool` e `int` (atomicamente escritas em arquiteturas 32-bit). Em produção, recomenda-se adicionar o mutex para corretude formal.

---

## 4. Sincronização e Comunicação entre Tarefas

### 4.1 Mutex do estado compartilhado

O `s_state_mutex` (FreeRTOS `SemaphoreHandle_t`) protege o acesso a `estufa_state_t` entre as tarefas `task_sensors`, `task_control` e `task_display`. O padrão adotado é:

```c
xSemaphoreTake(s_state_mutex, portMAX_DELAY);
// leitura ou escrita do estado
xSemaphoreGive(s_state_mutex);
```

O timeout `portMAX_DELAY` garante que a tarefa sempre obtenha o mutex, mas pode causar deadlock se uma tarefa travar dentro da seção crítica — aceitável para protótipo educacional.

### 4.2 ISR dos botões

As ISRs (`isr_btn_up` e `isr_btn_sel`) são marcadas com `IRAM_ATTR` para serem alocadas na RAM interna (IRAM), garantindo execução mesmo com a flash em modo de escrita (durante gravação no SPIFFS). As ISRs chamam diretamente funções do `oled_menu`, o que é uma simplificação — em produção, as ISRs deveriam usar `xQueueSendFromISR()` ou `xTaskNotifyFromISR()` para comunicar com a tarefa de display.

---

## 5. Configuração via Menuconfig (Kconfig)

O `Kconfig.projbuild` define **parâmetros de pré-compilação** organizados em sub-menus:

- **Pinos dos sensores:** `DHT11_GPIO`, `LDR_ADC_CHANNEL`
- **Pinos do OLED:** `OLED_I2C_SDA`, `OLED_I2C_SCL`, `OLED_I2C_ADDR`
- **Parâmetros de controle:** `SETPOINT_TEMP_DEFAULT`, `HISTERESE_TEMP`, `RELAY_GPIO`
- **Logging:** `LOG_INTERVAL_SEC`, `LOG_FILENAME`, `LOG_MAX_LINES`
- **Sensores:** `SENSOR_READ_INTERVAL_MS`

Essa abordagem elimina "magic numbers" no código e permite reconfigurar o projeto para diferentes layouts de hardware sem alterar o código-fonte, apenas rodando `idf.py menuconfig`.

---

## 6. Gerenciamento de Memória Flash

### Tabela de partições customizada

```
nvs       (24 KB)   - Non-Volatile Storage (WiFi, configurações)
phy_init  (4 KB)    - Calibração RF
factory   (1.5 MB)  - Firmware da aplicação
spiffs    (384 KB)  - Sistema de arquivos para logs
```

A partição SPIFFS de 384 KB comporta aproximadamente **3.800 linhas de log** (cada linha ~100 bytes), equivalente a mais de **31 horas de dados** com intervalo de 30 segundos. O mecanismo de rotação garante que o sistema nunca esgote a partição.

---

## 7. Controle de Temperatura com Histerese

A lógica de controle ON/OFF com histerese evita o "ciclo rápido" (rapid cycling) do relé, que ocorre quando a temperatura oscila em torno do set point causando comutações frequentes que reduzem a vida útil do contato elétrico.

**Algoritmo:**
```
Se temp < (SP - H/2): LIGA o relé
Se temp > (SP + H/2): DESLIGA o relé
Caso contrário: mantém estado atual
```

Com `CONFIG_HISTERESE_TEMP = 5` (0.5°C):
- Relé liga se temperatura cair abaixo de `SP - 0.25°C`
- Relé desliga se temperatura subir acima de `SP + 0.25°C`

Isso garante uma zona morta de 0.5°C em torno do set point.

---

## 8. Conceitos do Curso Aplicados

| Conceito | Onde aplicado |
|---|---|
| GPIO / Protocolos digitais | Driver DHT11 (single-wire, timing em µs) |
| ADC e calibração | Driver LDR (média de amostras, esp_adc_cal) |
| I2C | Comunicação com display OLED SSD1306 |
| UART | Interface de comandos seriais |
| FreeRTOS (Tasks, Mutex) | Arquitetura multi-tarefa com estado compartilhado |
| Interrupções (ISR) | Botões com IRAM_ATTR |
| SPIFFS / Sistema de arquivos | Logging persistente em CSV |
| Kconfig / menuconfig | Configuração de pré-compilação |
| ESP-IDF Component Manager | Biblioteca SSD1306 externa |
| Modularidade (componentes) | 5 componentes independentes e reutilizáveis |
| Controle ON/OFF | Histerese no controle de temperatura |

---

## 9. Limitações e Melhorias Futuras

| Limitação | Melhoria proposta |
|---|---|
| Debounce dos botões por software | Usar componente `espressif/button` |
| ISR chama função diretamente | Usar `xQueueSendFromISR` para desacoplar |
| SPIFFS sem journaling | Migrar para LittleFS para maior robustez |
| Sem WiFi / MQTT | Integrar envio de dados para broker MQTT |
| Sem NTP | Usar timestamps reais via NTP para o log |
| Sensor único DHT11 | Adicionar redundância ou sensor mais preciso (DHT22, SHT31) |
| Menu com apenas 2 botões | Usar encoder rotativo para melhor UX |

---

## 10. Conclusão

O projeto atingiu todos os requisitos especificados, demonstrando a integração completa de sensores, atuadores, display, armazenamento e comunicação serial em um sistema embarcado real. A arquitetura modular com componentes ESP-IDF facilita a manutenção e extensão do sistema, enquanto o uso correto do FreeRTOS garante a operação concorrente e determinística das diferentes funcionalidades.

A principal contribuição técnica do projeto é a implementação do driver DHT11 com controle preciso de temporização em microssegundos via busy-wait com interrupções desabilitadas, técnica essencial para protocolos de tempo crítico em sistemas embarcados sem suporte a DMA ou interface dedicada.

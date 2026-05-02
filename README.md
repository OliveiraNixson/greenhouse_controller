#  Sistema de Monitoramento de Estufa — Franzininho WiFi LAB01

> Projeto final do curso de Sistemas Embarcados com ESP-IDF.
> Monitora temperatura, umidade e luminosidade; controla um relé de aquecimento;
> exibe dados em OLED interativo e registra logs em memória flash.

---

## 📋 Índice

- [Descrição](#descrição)
- [Hardware necessário](#hardware-necessário)
- [Diagrama de conexões](#diagrama-de-conexões)
- [Instalação e configuração](#instalação-e-configuração)
- [Como usar](#como-usar)
- [Comandos seriais](#comandos-seriais)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Exemplos de funcionamento](#exemplos-de-funcionamento)

---

## Descrição

O sistema embarcado realiza as seguintes funções:

| Função | Descrição |
|---|---|
| **Monitoramento** | Lê temperatura e umidade (DHT11) e luminosidade (LDR/ADC) |
| **Controle** | Aciona relé com histerese quando temperatura < set point |
| **Display** | Exibe dados em tempo real no OLED 128×64 via I2C |
| **Menu** | Navega entre telas com 2 botões para ajustar configurações |
| **Logging** | Registra leituras em CSV no SPIFFS (flash interna) |
| **Serial** | Interface de comandos via UART0 (115200 bps) |

---

## Hardware necessário

| Componente | Quantidade | Observação |
|---|---|---|
| Franzininho WiFi LAB01 | 1 | ESP32-S2 / ESP32 |
| Sensor DHT11 | 1 | Com resistor pull-up 4k7 |
| LDR + Resistor 10 kΩ | 1 conjunto | Divisor de tensão |
| Display OLED SSD1306 | 1 | 128×64, I2C |
| Módulo relé 5V | 1 | Para controle de aquecedor |
| Botão tactile | 2 | BTN_UP e BTN_SEL |
| LED + Resistor 330 Ω | 1 | Indicador do relé (opcional) |
| Protoboard + cabos | — | |

---

## Diagrama de conexões

```
Franzininho WiFi LAB01
         │
         ├── GPIO4  ────────────────► DHT11 (DATA)
         │                              ├── VCC (3.3V)
         │                              └── GND
         │
         ├── GPIO36 (ADC1 CH0) ─────► LDR ──┤ (divisor com 10kΩ para GND)
         │                                   └── VCC (3.3V)
         │
         ├── GPIO21 (SDA) ──────────► OLED SSD1306 (SDA)
         ├── GPIO22 (SCL) ──────────► OLED SSD1306 (SCL)
         │                              ├── VCC (3.3V)
         │                              └── GND
         │
         ├── GPIO26 ─────────────────► Relé (IN)
         │                              ├── VCC (5V externo)
         │                              └── GND
         │
         ├── GPIO0  ─────────────────► BTN_UP (BOOT button)
         └── GPIO35 ─────────────────► BTN_SEL (pull-up interno)
```

### Diagrama de blocos do hardware

```
┌─────────────────────────────────────────────────────────────────┐
│                     Franzininho WiFi LAB01                      │
│                                                                 │
│  ┌──────────┐     ┌─────────────┐     ┌──────────────────────┐ │
│  │  DHT11   │────►│  GPIO4      │     │   FreeRTOS Tasks     │ │
│  │ Temp/Umid│     │  (1-Wire)   │     │                      │ │
│  └──────────┘     └──────┬──────┘     │  task_sensors  (C0)  │ │
│                          │            │  task_control  (C0)  │ │
│  ┌──────────┐     ┌──────▼──────┐     │  task_display  (C1)  │ │
│  │   LDR    │────►│  ADC1 CH0   │────►│  task_logging  (C0)  │ │
│  │ Luminosi │     │  GPIO36     │     │  serial_cmd    (C0)  │ │
│  └──────────┘     └─────────────┘     └──────────────────────┘ │
│                                                │                │
│  ┌──────────┐     ┌─────────────┐             │                │
│  │   OLED   │◄────│  I2C Master │◄────────────┤                │
│  │ SSD1306  │     │  SDA/SCL    │             │                │
│  └──────────┘     └─────────────┘             │                │
│                                               │                │
│  ┌──────────┐     ┌─────────────┐             │                │
│  │   Relé   │◄────│  GPIO26     │◄────────────┤                │
│  │ Aquecedor│     │  (output)   │             │                │
│  └──────────┘     └─────────────┘             │                │
│                                               │                │
│  ┌──────────┐     ┌─────────────┐             │                │
│  │  SPIFFS  │◄───►│  Flash SPI  │◄────────────┘                │
│  │  log.txt │     │  (1 MB)     │                              │
│  └──────────┘     └─────────────┘                              │
│                                                                 │
│  ┌──────────┐     ┌─────────────┐                              │
│  │  UART0   │◄───►│ Serial Cmd  │                              │
│  │ 115200bps│     │  Interface  │                              │
│  └──────────┘     └─────────────┘                              │
└─────────────────────────────────────────────────────────────────┘
```

---

## Instalação e configuração

### Pré-requisitos

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) instalado e configurado
- Python 3.8+
- Terminal serial: **minicom**, **PuTTY** ou **idf.py monitor**

### Clonar o repositório

```bash
git clone https://github.com/seu-usuario/greenhouse-controllerlerler.git
cd greenhouse-controllerlerlerler
```

### Instalar dependências (Component Manager)

```bash
idf.py add-dependency "espressif/ssd1306^1.0.1"
```

### Configurar via menuconfig

```bash
idf.py menuconfig
```

Navegue até **"Configurações da Estufa"** e ajuste:

| Opção | Padrão | Descrição |
|---|---|---|
| `DHT11_GPIO` | 4 | GPIO do sensor DHT11 |
| `LDR_ADC_CHANNEL` | 0 (GPIO36) | Canal ADC do LDR |
| `OLED_I2C_SDA` | 21 | Pino SDA do OLED |
| `OLED_I2C_SCL` | 22 | Pino SCL do OLED |
| `SETPOINT_TEMP_DEFAULT` | 25°C | Temperatura alvo inicial |
| `RELAY_GPIO` | 26 | GPIO do relé |
| `LOG_INTERVAL_SEC` | 30 | Intervalo de logging |

### Compilar e gravar

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Como usar

### Navegação no menu OLED

```
Tela MONITOR (padrão)
  │
  └── [BTN_SEL] ──► MENU PRINCIPAL
                        │
                        ├── Ajustar Temp ──► [BTN_UP] incrementa °C
                        │                   [BTN_SEL] confirma e volta
                        │
                        ├── Configurações ──► [BTN_UP] move cursor
                        │                    [BTN_SEL] toggle ON/OFF
                        │
                        └── Voltar ──► Tela MONITOR
```

### Tela de monitoramento

```
=== ESTUFA ===
Temp:   26.5 C
Umid:   62.0 %
Luz:      78 %
SP:  25 C  [ON]
CTRL:ON  LOG:ON
```

---

## Comandos seriais

Abra o terminal a **115200 bps** e digite:

| Comando | Descrição |
|---|---|
| `help` | Lista todos os comandos |
| `status` | Estado atual da estufa |
| `dump` | Exibe o arquivo de log completo |
| `clear` | Apaga o arquivo de log |
| `log on` / `log off` | Habilita/desabilita logging |
| `ctrl on` / `ctrl off` | Liga/desliga controle do relé |
| `sp 28` | Define set point para 28°C |

**Exemplo de sessão:**
```
> status

=== STATUS DA ESTUFA ===
  Temperatura : 26.5 °C
  Umidade     : 62.0 %
  Luminosidade: 78 %
  Set Point   : 25 °C
  Relé        : LIGADO
  Controle    : ATIVO
  Logging     : ATIVO (47 linhas)
========================

> dump

=== DUMP DO LOG: /spiffs/log.txt ===
timestamp_ms,temperatura,umidade,luminosidade,setpoint,relay
32000,25.5,63.0,75,25,0
62000,26.1,62.5,76,25,1
92000,26.5,62.0,78,25,1
=== FIM DO LOG (4 linhas) ===
```

---

## Estrutura do projeto

```
greenhouse-controllerlerlerler/
├── CMakeLists.txt              # Build raiz
├── Kconfig.projbuild           # Opções do menuconfig
├── partitions.csv              # Tabela de partições com SPIFFS
├── sdkconfig.defaults          # Configurações padrão ESP-IDF
│
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml       # Dependências externas
│   └── main.c                  # Ponto de entrada, tarefas FreeRTOS
│
├── components/
│   ├── dht11/                  # Driver do sensor DHT11
│   │   ├── include/dht11.h
│   │   ├── dht11.c
│   │   └── CMakeLists.txt
│   │
│   ├── ldr/                    # Driver do LDR via ADC
│   │   ├── include/ldr.h
│   │   ├── ldr.c
│   │   └── CMakeLists.txt
│   │
│   ├── oled_menu/              # Display OLED + menu interativo
│   │   ├── include/oled_menu.h
│   │   ├── oled_menu.c
│   │   └── CMakeLists.txt
│   │
│   ├── data_logger/            # Logging em SPIFFS
│   │   ├── include/data_logger.h
│   │   ├── data_logger.c
│   │   └── CMakeLists.txt
│   │
│   └── serial_cmd/             # Interface de comandos UART
│       ├── include/serial_cmd.h
│       ├── serial_cmd.c
│       └── CMakeLists.txt
│
└── docs/
    └── relatorio_tecnico.md
```

---

## Exemplos de funcionamento

### Log CSV gerado

```csv
timestamp_ms,temperatura,umidade,luminosidade,setpoint,relay
30000,24.5,65.0,80,25,1
60000,25.2,64.5,80,25,0
90000,24.8,65.2,78,25,1
```

### Controle de temperatura com histerese

Com set point = 25°C e histerese = 0.5°C:
- Temp < 24.5°C → relé **LIGA** (aquece)
- Temp > 25.5°C → relé **DESLIGA**
- Temp entre 24.5–25.5°C → **mantém estado atual**

---

## Licença

MIT License — livre para uso educacional e comercial.

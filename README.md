# Monza ALDL Dashboard

Dashboard embarcado para leitura ALDL da ECU Multec 700 do Chevrolet Monza EFI, usando ESP32, display TFT, sensores I2C, cartão SD e atualização via OTA.

O objetivo do projeto é criar um painel auxiliar para monitorar dados da ECU em tempo real, visualizar sensores importantes, diagnosticar comunicação ALDL e auxiliar na manutenção do carro.

---

## Visão geral

Este projeto foi desenvolvido para um Chevrolet Monza EFI com comunicação ALDL em 8192 baud.

O sistema utiliza um ESP32 para se comunicar com a ECU, processar os dados recebidos e exibir as informações em um display TFT ST7789. Além disso, possui menu navegável por encoder, suporte a cartão SD para configuração, sensores auxiliares via I2C e atualização de firmware via OTA em modo Access Point.

---

## Funcionalidades

- Comunicação ALDL em 8192 baud
- Leitura de dados da ECU Multec 700
- Interface gráfica em display TFT ST7789
- Menu navegável por encoder rotativo
- Leitura de sensores da ECU em tempo real
- Diagnóstico da comunicação ALDL
- Leitura de códigos de erro da ECU
- Comando para limpar códigos de erro da ECU
- Diagnóstico dos dispositivos I2C
- Diagnóstico do cartão SD
- Configuração de brilho da tela
- Configuração de buzzer
- GIF de abertura carregado pelo cartão SD
- Atualização de firmware via OTA em modo Access Point

---

## Telas disponíveis

### Sensores

- TPS
- ADMAP / MAP em volts
- ADBARO / Barométrico em volts
- CTS / temperatura do motor
- IAT / temperatura de admissão
- Voltagem da bateria
- RPM
- Tempo de injeção
- CO2 POT em volts

### Diagnóstico

- Status ALDL
- Códigos ECU
- Limpar erros ECU
- Status I2C
- Status iluminação
- Status SD Card
- Teste do buzzer
- Teste do display

### Configuração

- Data e hora
- GIF de abertura
- Alertas
- Brilho da tela
- Dashboard padrão
- Buzzer
- Update via OTA

---

## Hardware utilizado

- ESP32
- Display TFT ST7789 1.69" 280x240
- Módulo RTC DS3231
- Sensor BME280
- Sensor MPU6050 / GY-521
- Módulo cartão SD
- Encoder rotativo KY-040
- Buzzer passivo
- Conversor buck LM2596S
- Interface ALDL com transistor 2N2222A
- Resistores para interface ALDL
- Capacitores para filtragem de alimentação

---

## Pinagem

| Função | Pino ESP32 |
|---|---:|
| TFT CS | GPIO 5 |
| TFT DC | GPIO 2 |
| TFT RST | GPIO 4 |
| TFT BLK | GPIO 32 |
| SPI MOSI | GPIO 23 |
| SPI CLK | GPIO 18 |
| SD CS | GPIO 13 |
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |
| Encoder CLK | GPIO 25 |
| Encoder DT | GPIO 26 |
| Encoder SW | GPIO 27 |
| Buzzer | GPIO 33 |
| Iluminação | GPIO 34 |
| ALDL RX | GPIO 16 |
| ALDL TX | GPIO 17 |

---

## Comunicação ALDL

O projeto utiliza comunicação ALDL com a ECU Multec 700 em 8192 baud.

### Frames utilizados

| Função | Frame |
|---|---|
| Shutup / inicialização | `F4 56 08 AE` |
| Requisição de stream | `F4 57 01 00 B4` |
| Limpar códigos ECU | `F4 57 0A 00 AB` |

---

## Leituras ALDL usadas

| Byte | Nome | Descrição | Uso no projeto |
|---:|---|---|---|
| 25 | ADBARO | Barometric pressure A/D counts | Barométrico em volts |
| 26 | ADMAP | Manifold pressure A/D counts | MAP em volts |
| 28 | ADVAC | Vacuum A/D counts | Removido / não utilizado |

---

## ADMAP, ADBARO e ADVAC

### ADMAP

O `ADMAP` é a leitura A/D do sensor MAP, representando a pressão absoluta no coletor de admissão.

No projeto, ele é convertido para volts e usado para acompanhar o comportamento do MAP em tempo real.

```cpp
valorMAP = mapRaw * (5.0f / 255.0f);
```

### ADBARO

O `ADBARO` é a leitura A/D da referência barométrica.

No projeto, ele é exibido como valor barométrico em volts.

```cpp
valorBarometrico = baroRaw * (5.0f / 255.0f);
```

### ADVAC

O `ADVAC` representa vácuo em A/D counts.

Ele não é usado atualmente no projeto, pois o foco é acompanhar o valor direto do ADMAP em volts.

---

## Arquivo de configuração

O projeto utiliza um arquivo `config.json` no cartão SD para salvar configurações do painel.

Exemplo:

```json
{
  "brilhoDia": 255,
  "brilhoNoite": 255,
  "gifAbertura": "abertura.gif",
  "aldlPollingMs": 100,
  "modoBuzzer": 1
}
```

---

## OTA

O sistema possui atualização via OTA em modo Access Point.

Ao acessar a tela de update, o ESP32 cria uma rede Wi-Fi própria para envio do novo firmware.

Configuração padrão:

| Campo | Valor |
|---|---|
| SSID | `MonzaDash-OTA` |
| Senha | `12345678` |
| IP | `192.168.4.1` |

---

## Estrutura do projeto

```text
monza-aldl-dashboard/
├── sketch/
│   ├── monza-aldl-dashboard.ino
│   └── icones.h
├── .gitignore
├── LICENSE
└── README.md
```

---

## Status do projeto

Projeto em desenvolvimento.

Funcionalidades já implementadas:

- Menu principal
- Telas de sensores
- Diagnóstico ALDL
- Diagnóstico I2C
- Diagnóstico SD
- Leitura de códigos de erro
- Limpeza de códigos de erro
- Atualização OTA
- Configuração em cartão SD

Próximas melhorias previstas:

- Tela de alertas configuráveis
- Melhorias visuais no dashboard principal
- Registro de logs em SD
- Mais opções de configuração pelo menu
- Documentação do circuito ALDL
- Fotos e diagramas de montagem

---

## Aviso

Este projeto é experimental e foi desenvolvido para uso automotivo personalizado.

Use por sua conta e risco. Ligações incorretas na rede ALDL, alimentação, aterramento ou sensores podem causar mau funcionamento no veículo ou no módulo eletrônico.

Antes de instalar no carro, revise o circuito, valide as tensões e teste a comunicação em bancada sempre que possível.

---

## Licença

Este projeto está licenciado sob a licença MIT.

Consulte o arquivo `LICENSE` para mais detalhes.

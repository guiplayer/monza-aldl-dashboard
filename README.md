# Monza ALDL Dashboard

Dashboard embarcado para leitura, diagnóstico e visualização de dados ALDL da ECU Multec 700 do Chevrolet Monza GLS 2.0 EFI, usando ESP32, display TFT ST7789V3, encoder rotativo, cartão SD, sensores auxiliares, alertas configuráveis, GIF de abertura e atualização via OTA.

O projeto foi criado para transformar os dados da ECU e dos sensores externos em uma interface visual instalada no carro, com navegação por menu, dashboards, telas de diagnóstico e configurações persistentes no cartão SD.

---

## Visão geral

O **Monza ALDL Dashboard** lê dados da ECU via protocolo ALDL em 8192 baud e exibe as informações em tempo real no display TFT.

Entre as informações exibidas estão:

- RPM
- TPS
- MAP em volts
- Temperatura do motor, CTS
- Temperatura de admissão, IAT
- Tensão da bateria
- Tempo de injeção
- Velocidade
- Potenciômetro de CO2
- Status da ECU
- Pacotes ALDL recebidos
- Erros de checksum
- Tempo de motor ligado
- Fan estimado pela temperatura
- Sensores BME280 e MPU6050
- Status do cartão SD
- Status elétrico da iluminação

O sistema também possui:

- Menu navegável por encoder
- Dashboards múltiplos
- Alertas visuais configuráveis
- Configurações salvas em `config.json` no cartão SD
- GIF de abertura salvo no SD
- Upload de GIFs via página web
- Atualização OTA via página web
- Diagnóstico ALDL
- Leitura e limpeza de códigos da ECU

---

## Hardware principal

### Placa

- ESP32

### Display

- TFT ST7789V3
- Resolução usada: `280x240`
- Comunicação SPI

### Controle

- Encoder rotativo KY-040
- Botão do encoder para selecionar, confirmar e voltar

### Sensores e módulos

- BME280 via I2C
- MPU6050 via I2C
- RTC DS3231 via I2C
- Módulo SD via SPI
- Buzzer passivo
- Entrada de iluminação do painel

### Armazenamento

- Cartão SD
- Formato recomendado: **FAT32**

> Importante: no Windows 11, cartões maiores podem ser formatados como exFAT por padrão. Para o ESP32, use FAT32.

### Comunicação ALDL

- Serial2 do ESP32
- Baud rate: `8192`
- Comunicação com a ECU Multec 700 pelo pino ALDL do veículo

---

## Pinagem

| Função | GPIO |
|---|---:|
| TFT CS | 5 |
| TFT DC | 2 |
| TFT RST | 4 |
| TFT BLK | 32 |
| SPI MOSI | 23 |
| SPI CLK | 18 |
| SD CS | 13 |
| I2C SDA | 21 |
| I2C SCL | 22 |
| Encoder CLK | 25 |
| Encoder DT | 26 |
| Encoder SW | 27 |
| Buzzer | 33 |
| Iluminação | 34 |
| Shift Light / entrada futura | 35 |
| ALDL RX2 | 16 |
| ALDL TX2 | 17 |

---

## Bibliotecas utilizadas

O projeto usa as seguintes bibliotecas principais:

```cpp
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <AnimatedGIF.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
```

---

## Protocolo ALDL

O projeto usa a Serial2 do ESP32 para comunicação ALDL com a ECU.

Configuração principal:

```cpp
static const uint32_t ALDL_BAUD_RATE = 8192;
```

Frames principais:

```cpp
static const uint8_t ALDL_REQ_SHUTUP[] = { 0xF4, 0x56, 0x08, 0xAE };
static const uint8_t ALDL_REQ_STREAM[] = { 0xF4, 0x57, 0x01, 0x00, 0xB4 };
static const uint8_t ALDL_REQ_CLEAR_DTC[] = { 0xF4, 0x57, 0x0A, 0x00, 0xAB };
```

O fluxo ALDL faz:

1. Inicialização da sessão ALDL.
2. Envio do frame `shutup`.
3. Limpeza do buffer serial.
4. Polling periódico da ECU.
5. Validação do checksum.
6. Processamento do payload.
7. Atualização das variáveis exibidas na interface.

---

## Dados ALDL exibidos

| Dado | Descrição |
|---|---|
| RPM | Rotação do motor |
| TPS | Abertura da borboleta |
| MAP | Tensão do sensor MAP |
| CTS | Temperatura do motor |
| IAT | Temperatura de admissão |
| Bateria | Tensão da bateria |
| Tempo de injeção | Tempo de abertura do bico |
| CO2 POT | Tensão do potenciômetro de CO2 |
| Velocidade | Velocidade lida da ECU |
| Malf1 / Malf2 / Malf3 | Bytes de falha da ECU |
| Tempo motor ligado | Tempo de funcionamento lido do stream |
| ECU online | Status de comunicação ALDL |

---

## Menus

A interface é organizada em menus.

### Menu principal

- Dashboard
- Sensores
- Diagnóstico
- Configuração

### Sensores

- TPS
- MAP
- CTS, temperatura do motor
- IAT, temperatura de admissão
- Voltímetro
- RPM
- Tempo de injeção
- CO2 POT
- Voltar

### Diagnóstico

- Status ALDL
- Códigos ECU
- Limpar erros ECU
- Status MPU
- Status BME
- Status I2C
- Status Iluminação
- SD Card
- Teste Buzzer
- Teste Display
- Voltar

### Configuração

- Data e hora
- GIF abertura
- Alertas
- Brilho tela
- Dash default
- Buzzer
- Update via OTA
- Upload Gifs
- Voltar

---

## Dashboards

O projeto possui múltiplos dashboards navegáveis pelo encoder.

### Dash 1 - Ambiente

Exibe:

- Hora
- Data
- Temperatura ambiente
- Umidade
- Pressão atmosférica

### Dash 2 - Motor

Exibe:

- RPM
- TPS
- MAP
- Bateria

### Dash 3 - Sensores

Exibe:

- Temperatura do motor
- Temperatura de admissão
- Tempo de injeção
- CO2 POT

### Dash 4 - ECU

Exibe:

- Velocidade
- Status da ECU
- Pacotes recebidos
- Erros de checksum

### Dash 5 - Custom

Exibe:

- RPM
- TPS
- MAP
- Velocidade

### Dash 6 - Status

Exibe:

- Fan estimado
- Motor ligado
- Tempo de motor ligado
- ECU online/offline

---

## Fan estimado

A flag de fan via ALDL não se mostrou confiável como status real no Monza, então o projeto usa um status estimado pela temperatura do motor.

Exemplo:

```cpp
bool fanEstimado = tempMotor >= fanEstimadoLimite;
```

Por padrão:

```cpp
float fanEstimadoLimite = 100.0;
```

Esse status é exibido como indicador visual, não como confirmação real de acionamento elétrico da ventoinha.

---

## Alertas

O sistema possui alertas configuráveis pela tela **Alertas**.

Alertas disponíveis:

- Temperatura alta do motor
- Tensão baixa da bateria
- Shift Light por RPM

Configurações principais:

```cpp
bool alertaTempMotorAtivo = true;
bool alertaTensaoAtivo = true;
bool alertaShiftLightAtivo = true;

float alertaTempMotorLimite = 105.0;
float alertaTensaoMinima = 11.5;
int alertaShiftLightRPM = 5500;
```

Os alertas só são disparados quando:

- O sistema está em uma tela ALDL ou Dashboard.
- Já existe frame ALDL válido.
- A ECU está conectada.
- A condição configurada foi atingida.

Quando um alerta é disparado, ele aparece como tela cheia para evitar mistura visual com os dados ALDL.

---

## Shift Light

O Shift Light é calculado por RPM configurável.

Exemplo:

```cpp
if (alertaShiftLightAtivo && valorRPM >= alertaShiftLightRPM) {
  desenharAlertaTela("SHIFT", "TROCAR MARCHA", ST77XX_YELLOW);
}
```

Isso evita depender de uma flag ALDL que pode não representar o status real no veículo.

---

## Configurações salvas no SD

O projeto salva as configurações no arquivo:

```txt
/config.json
```

Configurações salvas:

- Brilho dia
- Brilho noite
- GIF de abertura
- Frequências do buzzer
- Modo do buzzer
- Polling ALDL
- Dashboard inicial
- Alertas ativos
- Limite de temperatura
- Tensão mínima
- RPM do Shift Light

Exemplo de estrutura:

```json
{
  "brilhoDia": 255,
  "brilhoNoite": 80,
  "gif": "abertura.gif",
  "fEnc": 1200,
  "fCli": 2000,
  "fSuc": 2500,
  "fErr": 800,
  "modoBuzzer": 1,
  "aldlPollingMs": 100,
  "dashInicialDefault": -1,
  "alertaTempMotorAtivo": true,
  "alertaTensaoAtivo": true,
  "alertaShiftLightAtivo": true,
  "alertaTempMotorLimite": 105.0,
  "alertaTensaoMinima": 11.5,
  "alertaShiftLightRPM": 5500
}
```

---

## Dashboard inicial

Na tela **Dash default**, é possível escolher se o projeto deve iniciar diretamente em um dashboard ao ligar.

Opções:

| Valor | Comportamento |
|---:|---|
| -1 | Nenhum, abre no menu principal |
| 0 | Dash 1 - Ambiente |
| 1 | Dash 2 - Motor |
| 2 | Dash 3 - Sensores |
| 3 | Dash 4 - ECU |
| 4 | Dash 5 - Custom |
| 5 | Dash 6 - Status |

---

## GIF de abertura

O projeto pode exibir um GIF de abertura salvo no cartão SD.

Recomendações:

- Formato: `.gif`
- Resolução: `280x240`
- Salvar na raiz do SD
- Usar nomes simples, sem espaços ou caracteres especiais

Exemplos:

```txt
/abertura.gif
/monza.gif
/gm.gif
```

A tela **GIF abertura** lista os arquivos `.gif` encontrados no SD e permite selecionar qual será exibido ao iniciar.

---

## Upload de GIFs via Wi-Fi

A tela **Upload Gifs** inicia um Access Point no ESP32 e permite enviar arquivos `.gif` pelo navegador.

Configuração padrão:

```cpp
String otaSSID = "MonzaDash-OTA";
String otaSenha = "12345678";
```

Fluxo de uso:

1. Entrar em `Configuração > Upload Gifs`.
2. Conectar no Wi-Fi `MonzaDash-OTA`.
3. Abrir no navegador:

```txt
http://192.168.4.1
```

4. Selecionar um arquivo `.gif`.
5. Enviar.
6. O arquivo será salvo no cartão SD.
7. Depois selecionar em `Configuração > GIF abertura`.

---

## Atualização OTA

A tela **Update via OTA** inicia o modo Access Point e permite atualizar o firmware `.bin` pelo navegador.

Fluxo de uso:

1. Entrar em `Configuração > Update via OTA`.
2. Conectar no Wi-Fi `MonzaDash-OTA`.
3. Abrir no navegador:

```txt
http://192.168.4.1
```

4. Selecionar o arquivo `.bin`.
5. Enviar.
6. O ESP32 reinicia após a atualização.

---

## Buzzer

O sistema usa buzzer para feedback de navegação e eventos.

Configurações disponíveis:

- Mudo
- Simples
- Duplo
- Frequência do encoder
- Frequência de clique
- Frequência de sucesso
- Frequência de erro

Pino usado:

```cpp
#define PIN_BUZZER 33
```

---

## Iluminação e brilho

O projeto lê o sinal de iluminação no GPIO 34:

```cpp
#define PIN_ILUMINACAO 34
```

Esse sinal pode ser usado para diferenciar brilho de dia e brilho de noite.

Configurações:

```cpp
int brilhoDia = 255;
int brilhoNoite = 255;
```

---

## Sensores auxiliares

### BME280

Usado para:

- Temperatura ambiente
- Umidade
- Pressão atmosférica
- Altitude estimada

Endereço I2C usado:

```txt
0x76
```

### MPU6050

Usado para:

- Acelerômetro
- Giroscópio
- Temperatura interna do sensor

Endereço I2C usado:

```txt
0x69
```

### DS3231

Usado para manter data e hora.

Endereço comum:

```txt
0x68
```

---

## Cartão SD

O SD é usado para:

- `config.json`
- GIFs de abertura
- Futuro armazenamento de logs

Pino CS:

```cpp
#define SD_CS 13
```

Formato recomendado:

```txt
FAT32
```

Estrutura sugerida:

```txt
/
├── config.json
├── abertura.gif
├── monza.gif
├── gm.gif
└── outros_gifs.gif
```

---

## Compilação

O projeto pode ser compilado usando Arduino IDE ou GitHub Actions.

### Arduino IDE

Configuração recomendada:

- Board: ESP32 Dev Module
- Upload Speed: 921600 ou 115200
- Partition Scheme: compatível com OTA
- Flash Frequency: 80 MHz
- Core Debug Level: None

### Geração de `.bin`

Na Arduino IDE:

```txt
Sketch > Export Compiled Binary
```

O arquivo `.bin` gerado pode ser usado no OTA.

---

## OTA via release

Uma forma prática de distribuir firmware é gerar o `.bin` e anexar em uma release do GitHub.

Fluxo sugerido:

1. Compilar o projeto.
2. Gerar o `.bin`.
3. Criar uma release.
4. Anexar o `.bin`.
5. Baixar pelo celular ou notebook.
6. Enviar pelo OTA do dashboard.

---

## Observações importantes

### ALDL

O ALDL do Monza/Multec 700 pode variar de acordo com versão de ECU, calibração e XDF usado. Algumas flags podem existir no XDF mas não representar status real em tempo real no veículo.

Por isso, o projeto prioriza leituras confirmadas na prática, como:

- RPM
- TPS
- MAP
- CTS
- Bateria
- Tempo de injeção
- CO2 POT
- Velocidade
- Tempo de motor ligado

### Fan

O status de fan é estimado por temperatura, pois a flag analisada não alterou quando a ventoinha armou em testes com EFILive V4.

### Shift Light

O Shift Light é configurável por RPM, em vez de depender de flag ALDL.

### Tela

Para reduzir flicker, as telas devem evitar `fillScreen()` constante e preferir limpar apenas as áreas que mudam.

---

## Roadmap

Ideias futuras:

- Tela de tempos UI configuráveis
- Log de dados ALDL no SD
- Exportação CSV
- Tela gráfica de RPM, MAP e TPS
- Tela de diagnóstico de ruído ALDL
- Configuração do limite do fan estimado
- Melhor página web para gerenciar GIFs
- Exclusão de GIFs pelo navegador
- Seleção de GIF via página web
- Modo Wi-Fi client para buscar hora via NTP
- OTA em modo client além de AP mode

---

## Status do projeto

Em desenvolvimento.

Funcionalidades atuais:

- Leitura ALDL
- Dashboards
- Sensores auxiliares
- Configuração via menu
- Alertas
- OTA
- Upload de GIFs
- Configuração persistente via SD

---

## Autor

Projeto desenvolvido por Guilherme Monteiro para o Chevrolet Monza GLS 1996 2.0 EFI com ECU Multec 700.

---

## Aviso

Este projeto é experimental e automotivo. Faça ligações elétricas com cuidado, use proteção adequada contra ruído, fusíveis, aterramento correto e conversores de tensão confiáveis.

O uso no veículo é por conta e risco do instalador.

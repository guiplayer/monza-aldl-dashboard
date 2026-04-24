# Monza ALDL Dashboard

Dashboard embarcado para Chevrolet Monza EFI usando ESP32, comunicação ALDL 8192 baud, display TFT, sensores I2C, cartão SD, encoder rotativo e atualização OTA.

Projeto desenvolvido para um Chevrolet Monza GLS 1996 2.0 EFI gasolina com ECU Multec 700.

## Recursos

- Comunicação ALDL com ECU Multec 700
- Leitura de dados do motor via ALDL
- Display TFT ST7789 1.69" 280x240
- Menu com encoder rotativo KY-040
- Diagnóstico de códigos de erro da ECU
- Limpeza de códigos de erro da ECU
- Status de comunicação ALDL
- Status dos dispositivos I2C
- Leitura de sensores auxiliares
- Configurações salvas em cartão SD
- Atualização via OTA em modo AP
- Buzzer para alertas sonoros

## Hardware utilizado

- ESP32
- Display TFT ST7789V3 1.69" 280x240
- RTC DS3231
- Sensor BME280
- MPU-6050 / GY-521
- Módulo microSD
- Encoder rotativo KY-040
- Buzzer passivo
- Conversor buck LM2596S
- Transistor 2N2222A para interface ALDL

## Pinagem principal

| Função | GPIO |
|---|---:|
| ALDL RX | GPIO16 |
| ALDL TX | GPIO17 |
| TFT SCL | GPIO18 |
| TFT SDA | GPIO23 |
| TFT CS | definir no sketch |
| TFT DC | definir no sketch |
| TFT RES | definir no sketch |
| SD CS | GPIO13 |
| Encoder CLK | GPIO25 |
| Encoder DT | GPIO26 |
| Encoder SW | GPIO27 |
| Buzzer | GPIO33 |

## Comunicação ALDL

O projeto utiliza comunicação ALDL em 8192 baud, com interface baseada em transistor NPN 2N2222A.

A implementação atual considera:
- envio de comando para iniciar comunicação;
- filtragem de eco após transmissão;
- leitura de frame da ECU;
- validação de tamanho e checksum;
- extração de bytes do payload;
- leitura de parâmetros como TPS, MAP, CTS, IAT, RPM e tensão.

## Configuração

As configurações são armazenadas no cartão SD em:

```text
/config.json
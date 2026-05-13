# Monza ALDL Dashboard

Dashboard embarcado para Chevrolet Monza EFI com comunicação ALDL, display TFT ST7789, sensores auxiliares, logs em cartão SD, upload OTA e mascote animado.

O projeto foi desenvolvido para monitorar dados da ECU Multec 700 via ALDL 8192 baud, exibindo informações em tempo real no display TFT e permitindo diagnósticos, configuração, logs e animações diretamente no ESP32.

---

## 🚗 Veículo alvo

- Chevrolet Monza 1996
- Motor 2.0 EFI gasolina
- ECU Multec 700
- Comunicação ALDL 8192 baud
- Linha ALDL no pino M do conector de diagnóstico

---

## ✨ Principais recursos

- Comunicação ALDL com ECU Multec 700
- Dashboards com dados em tempo real
- Telas individuais de sensores
- Diagnóstico ALDL
- Leitura de códigos de falha da ECU
- Comando para limpar erros da ECU
- Status de SD, I2C, BME280, MPU6050 e alimentação
- Configuração de brilho
- Configuração de buzzer
- Configuração de alertas
- Dashboard inicial configurável
- Upload OTA de firmware via modo AP
- Upload de GIFs via navegador
- GIF de abertura configurável
- Redimensionamento automático de GIFs maiores que a tela
- Logs em CSV no cartão SD
- Limpeza automática de logs antigos
- Tela G-Force usando MPU6050
- Mascote / Monzagotchi com humor baseado nos dados do carro

---

## 🖥️ Display

O projeto usa um display TFT ST7789 SPI.

Configuração atual:

- Display ST7789
- Resolução usada no projeto: 280 x 240
- Biblioteca: Adafruit ST7789 / Adafruit GFX
- Backlight controlado pelo GPIO 32

---

## 📊 Dashboards

O sistema possui múltiplos dashboards acessíveis pelo menu principal.

### Dashboards atuais

1. **Relógio**
   - Hora
   - Data
   - Temperatura ambiente
   - Umidade
   - Pressão atmosférica

2. **Módulo**
   - ECU ID em hexadecimal
   - ECU ID em decimal
   - Código do módulo
   - Código GM

3. **Motor**
   - RPM
   - TPS
   - MAP
   - Temperatura do motor

4. **Motor 2**
   - Tensão da bateria
   - Velocidade
   - IAC Steps
   - Potenciômetro CO2

5. **Motor 3**
   - RPM
   - TPS
   - MAP
   - Velocidade

6. **Mistura**
   - Avanço de ignição / SPK
   - AFR desejado pela ECU
   - Tempo de injeção
   - Potenciômetro CO2

7. **Status**
   - FAN estimado
   - VMOTOR
   - Tempo de motor ligado
   - Temperatura de admissão

8. **Status ECU**
   - ECU online/offline
   - Pacotes recebidos
   - Erros de checksum
   - Bytes descartados

9. **G-Force**
   - Força lateral
   - Força longitudinal
   - Força vertical
   - G total

---

## 🧪 Sensores e dados ALDL

O projeto lê e exibe diversos dados vindos da ECU via ALDL.

### Telas individuais de sensores

- RPM
- TPS
- MAP
- CTS / temperatura do motor
- IAT / temperatura de admissão
- VBAT / tensão da bateria
- Velocidade
- IAC Steps
- Tempo de injeção
- Potenciômetro CO2
- SPK / avanço de ignição
- AFR desejado pela ECU
- FAN estimado
- VMOTOR
- Tempo de motor ligado

---

## ⚠️ Alertas

O sistema possui alertas configuráveis para:

- Temperatura alta do motor
- Tensão baixa da bateria
- Shift Light por RPM

Quando um alerta é acionado, uma tela de aviso é exibida.

Na tela do mascote, os alertas são representados pelo próprio humor do Monzagotchi.

---

## 🐾 Mascote / Monzagotchi

O projeto possui uma tela de mascote inspirada em Tamagotchi, chamada **Monzagotchi**.

O mascote fica no menu principal, junto com Dashboard, Sensores, Diagnóstico e Configuração.

Menu principal:

```txt
Dashboard
Sensores
Diagnostico
Mascote
Configuracao
```

A tela do mascote também ativa a comunicação ALDL por trás, para que ele reaja aos dados reais do carro.

### Estados do mascote

| Estado | Condição |
|---|---|
| Dormindo | Sem frame válido da ECU / aguardando comunicação |
| Feliz | Dados normais |
| Andando | Velocidade maior que 0 |
| Assustado | RPM acima do limite do Shift Light |
| Triste | ECU offline |
| Doente | Tensão baixa da bateria |
| Quente | Temperatura do motor acima do limite de alerta |

### Frases do mascote

Exemplos de falas usadas na tela:

- `ZzZ... esperando a ECU`
- `Tudo certo no Monza!`
- `Vrum vrum!`
- `EITA RPM!`
- `Cade a ECU?`
- `Bateria fraca :(`
- `To fritando!`

### GIFs do mascote

Os GIFs do mascote ficam na pasta:

```txt
/mascote/
```

Arquivos esperados:

```txt
/mascote/dormindo.gif
/mascote/feliz.gif
/mascote/andando.gif
/mascote/assustado.gif
/mascote/triste.gif
/mascote/doente.gif
/mascote/quente.gif
```

Se algum GIF ainda não foi enviado, a tela mostra um placeholder com o estado atual.

### Upload dos GIFs do mascote

Os GIFs podem ser enviados pelo próprio dashboard usando a tela:

```txt
Configuracao > Upload Gifs
```

O ESP32 cria uma rede Wi-Fi em modo AP. Depois basta acessar o IP informado na tela, normalmente:

```txt
192.168.4.1
```

Na página de upload existe uma área separada para enviar/substituir os GIFs de cada humor do mascote.

---

## 💾 Logs no cartão SD

O projeto possui gravação de logs em CSV no cartão SD.

Os logs são gravados somente quando:

- A opção de logs está ativada
- A tela atual usa dados ALDL
- A ECU está conectada
- Existe frame válido recebido
- O intervalo configurado foi atingido

Os arquivos são salvos na pasta:

```txt
/logs/
```

Formato do nome:

```txt
/logs/aldl_YYYYMMDD_HHMMSS.csv
```

Exemplo:

```txt
/logs/aldl_20260512_213410.csv
```

### Sensores disponíveis para log

É possível escolher quais dados serão gravados:

- RPM
- TPS
- MAP
- CTS / temperatura do motor
- VBAT / tensão da bateria
- Velocidade
- IAC Steps
- Tempo de injeção
- CO2 POT
- SPK / avanço de ignição
- AFR desejado
- FAN estimado
- VMOTOR
- Tempo de motor ligado

### Exemplo de CSV

```csv
millis,data,hora,rpm,tps_percent,map_v,cts_c,vbat_v,velocidade_kmh,iac_steps,injecao_ms,co2_v,spk_graus,afr_desejado,fan,vmotor,tempo_motor_s
123456,12/05/2026,21:34:10,920,0.4,1.28,87.5,13.8,0,34,2.15,1.82,12.3,14.7,OFF,ON,754
```

### Limpeza automática

Quando o cartão SD passa de aproximadamente **90% de uso**, o sistema começa a apagar os logs mais antigos até reduzir o uso para aproximadamente **80%**.

Isso evita que o cartão fique cheio durante o uso.

---

## 📡 Upload OTA

O projeto possui uma tela de atualização OTA via modo AP.

Caminho:

```txt
Configuracao > Update via OTA
```

O ESP32 cria uma rede Wi-Fi própria e permite enviar o firmware pelo navegador.

SSID padrão:

```txt
MonzaDash-OTA
```

Senha padrão:

```txt
12345678
```

IP padrão:

```txt
192.168.4.1
```

---

## 🖼️ Upload de GIFs

O projeto permite enviar GIFs para o cartão SD pelo navegador.

Caminho:

```txt
Configuracao > Upload Gifs
```

A tela de upload permite:

- Enviar GIF de abertura
- Listar GIFs no SD
- Visualizar prévia dos GIFs
- Ver dimensão dos GIFs
- Renomear GIFs
- Deletar GIFs
- Enviar/substituir GIFs do mascote por humor

---

## 🧩 Hardware utilizado

### 🖥️ Display TFT ST7789

Display TFT ST7789 SPI, 8 pinos.

| Display TFT | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO 18 / SPI Clock |
| SDA | GPIO 23 / SPI MOSI |
| RES | GPIO 4 |
| DC | GPIO 2 |
| CS | GPIO 5 |
| BLK | GPIO 32 ou 3.3V direto |

---

### 🔊 Buzzer passivo

| Buzzer | ESP32 |
|---|---|
| + | GPIO 33 |
| - | GND |

---

### ⏰ RTC DS3231

| DS3231 | ESP32 |
|---|---|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 5V |
| GND | GND |

---

### 🌡️ BME280

| BME280 | ESP32 |
|---|---|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 3.3V |
| GND | GND |

---

### 🧭 GY-521 / MPU6050

| GY-521 / MPU6050 | ESP32 |
|---|---|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 3.3V |
| GND | GND |

---

### 💾 MicroSD

| MicroSD | ESP32 |
|---|---|
| CLK / SCK | GPIO 18 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| CS | GPIO 13 |
| VCC | 3.3V |
| GND | GND |

---

### 🔘 Encoder KY-040

| KY-040 | ESP32 |
|---|---|
| CLK | GPIO 25 |
| DT | GPIO 26 |
| SW | GPIO 27 |
| VCC | 3.3V |
| GND | GND |

---

## 🚗 Ligações com o carro

| Sinal do carro | ESP32 / circuito |
|---|---|
| 5V pós-regulador | VIN do ESP32 |
| GND | GND do ESP32 |
| Iluminação 12V | GPIO 34 |
| ALDL RX | GPIO 16 |
| ALDL TX | GPIO 17 |

> Observação: a linha ALDL do carro não deve ser ligada diretamente ao ESP32 sem o circuito de interface/proteção usado no projeto.

---

## 🔋 Alimentação

A alimentação do projeto é feita a partir dos 12V do carro usando um conversor buck LM2596.

```txt
12V do carro -> LM2596
Saída 5V LM2596 -> VIN do ESP32
Saída 5V LM2596 -> DS3231
3.3V do ESP32 -> Display, BME280, MPU6050, MicroSD e Encoder
GND comum para todos os módulos
```

### Filtro contra ruído no LM2596

Para reduzir ruídos/interferências na alimentação e melhorar a estabilidade da leitura ALDL, foram adicionados capacitores no circuito do LM2596.

Capacitores utilizados:

| Capacitor | Posição sugerida | Função |
|---|---|---|
| 470µF / 25V eletrolítico | Entrada 12V do LM2596 | Ajuda a absorver quedas e oscilações da alimentação do carro |
| 10µF / 16V eletrolítico | Saída 5V do LM2596 | Ajuda a suavizar a saída de 5V |
| 100nF cerâmico | Próximo ao ESP32 e/ou sensores | Ajuda a filtrar ruído de alta frequência |
| 100nF cerâmico | Próximo ao módulo MicroSD | Ajuda na estabilidade durante leitura/gravação no SD |

Ligação recomendada:

```txt
12V carro  ----+---- LM2596 IN+
               |
             470µF
               |
GND carro  ----+---- LM2596 IN-

LM2596 OUT+ ----+---- VIN ESP32
                |
              10µF
                |
GND comum  -----+---- GND ESP32
```

Também é recomendado manter o GND comum entre:

- ESP32
- LM2596
- Display
- Sensores
- MicroSD
- RTC
- Circuito de interface ALDL

---

## 🔌 Barramento I2C

O projeto compartilha o mesmo barramento I2C entre:

- DS3231
- BME280
- MPU6050

Pinos usados:

```txt
SDA -> GPIO 21
SCL -> GPIO 22
```

Endereços comuns:

```txt
DS3231  -> 0x68
BME280  -> 0x76
MPU6050 -> 0x69
```

---

## 🧠 ALDL

Configuração da comunicação:

```txt
Baud rate: 8192
RX: GPIO 16
TX: GPIO 17
```

Comandos usados:

```txt
Shutup request:
F4 56 08 AE

Data stream request:
F4 57 01 00 B4

Clear DTC request:
F4 57 0A 00 AB
```

Frame esperado:

```txt
Header: F4 95 01
Payload: 63 bytes
Checksum: 1 byte
Total: 67 bytes
```

---

## 🧯 Códigos de falha monitorados

A tela de códigos ECU monitora falhas baseadas nos bytes de malfunction word.

Exemplos:

- Erro 12 - Sem pulsos de referência
- Erro 14 - Sensor de temperatura / curto massa
- Erro 15 - Sensor de temperatura / chicote
- Erro 21 - TPS voltagem alta
- Erro 22 - TPS voltagem baixa
- Erro 24 - Sem sinal sensor velocidade
- Erro 32 - Falha sistema EGR
- Erro 33 - MAP sinal alto
- Erro 34 - MAP sinal baixo
- Erro 35 - Atuador IAC
- Erro 42 - Módulo HEI
- Erro 51 - Unidade comando / MEM-CAL
- Erro 54 - Potenciômetro CO2
- Erro 55 - Unidade comando defeito

---

## ⚙️ Configurações salvas no SD

As configurações são salvas no arquivo:

```txt
/config.json
```

Exemplos de dados salvos:

- Brilho dia
- Brilho noite
- GIF de abertura
- Frequências do buzzer
- Modo do buzzer
- Polling ALDL
- Dashboard inicial
- Alertas
- Intervalos de atualização da UI
- Logs SD
- Sensores selecionados para log

---

## 📁 Estrutura sugerida do cartão SD

```txt
/
├── config.json
├── abertura.gif
├── logs/
│   ├── aldl_20260512_213410.csv
│   └── aldl_20260512_214020.csv
└── mascote/
    ├── dormindo.gif
    ├── feliz.gif
    ├── andando.gif
    ├── assustado.gif
    ├── triste.gif
    ├── doente.gif
    └── quente.gif
```

---

## 🛠️ Bibliotecas usadas

- Wire
- SPI
- SD
- ArduinoJson
- AnimatedGIF
- Adafruit GFX
- Adafruit ST7789
- Adafruit BME280
- Adafruit MPU6050
- Adafruit Sensor
- RTClib
- WiFi
- WebServer
- Update
- vector

---

## 📌 Observações importantes

- A linha ALDL precisa de circuito de interface/proteção.
- O ESP32 trabalha em 3.3V.
- Não ligar sinais automotivos de 12V diretamente no ESP32.
- Usar GND comum entre todos os módulos.
- O cartão SD deve estar formatado corretamente.
- GIFs muito grandes podem afetar desempenho.
- Para melhor desempenho, prefira GIFs pequenos e próximos da resolução usada na tela.
- O AFR exibido é o AFR desejado/calculado pela ECU, não leitura real de wideband.
- No Monza EFI sem sonda, alguns dados podem ser fixos ou estimados dependendo da ECU.

---

## 🧪 Status atual

O projeto está em desenvolvimento e testes reais no carro.

Funcionalidades já implementadas:

- Leitura ALDL
- Dashboards
- Telas de sensores
- Logs SD
- Upload de GIFs
- Mascote por humor
- OTA via AP
- G-Force
- Alertas
- Configurações persistentes no SD

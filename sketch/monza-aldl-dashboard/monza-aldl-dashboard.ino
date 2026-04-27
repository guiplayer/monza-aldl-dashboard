#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "icones.h"
#include <AnimatedGIF.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <RTClib.h>
#include <vector>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

// =======================
// CORES
// =======================
#define ST77XX_GREY 0x8410
#ifndef ST77XX_ORANGE
#define ST77XX_ORANGE 0xFD20
#endif
#ifndef ST77XX_MAGENTA
#define ST77XX_MAGENTA 0xF81F
#endif

// =======================
// PINOS (Configuração Original)
// =======================
#define TFT_CS     5
#define TFT_DC     2
#define TFT_RST    4
#define TFT_BLK    32
#define SPI_MOSI   23
#define SPI_CLK    18
#define SD_CS      13
#define SDA_PIN    21
#define SCL_PIN    22
#define ENC_CLK    25
#define ENC_DT     26
#define ENC_SW     27
#define PIN_BUZZER 33
#define PIN_ILUMINACAO  34
#define RX2 16
#define TX2 17

// =======================
// CANAIS
// =======================
#define CANAL_BRILHO 0
#define CANAL_BUZZER 2

// =======================
// IDENTIFICADORES DE TELAS (IDs)
// =======================
#define TELA_DASHBOARD        1
#define TELA_AJUSTAR_HORA     12
#define TELA_ALDL             13
#define TELA_TESTE_I2C        19
#define TELA_TESTE_ELET       20
#define TELA_BME              21
#define TELA_MPU              22
#define TELA_STATUS_SD        23
#define TELA_TESTE_BUZZER     24
#define TELA_TESTE_DISPLAY    25
#define TELA_AJUSTE_BRILHO    26
#define TELA_SELECIONAR_GIF   27
#define TELA_CONFIG_BUZZER    28
#define TELA_OTA              29
#define TELA_TPS              30
#define TELA_MAP              31
#define TELA_CTS              32
#define TELA_IAT              33
#define TELA_VOLT             34
#define TELA_RPM              35
#define TELA_TEMPO_INJECAO    36
#define TELA_CO2              37
#define TELA_CODIGOS_ECU      38
#define TELA_LIMPAR_ECU       39

// =======================
// OBJETOS
// =======================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_BME280 bme;
Adafruit_MPU6050 mpu;
RTC_DS3231 rtc;

File gifFile;
AnimatedGIF gif;

// =======================
// OTA AP MODE
// =======================
WebServer otaServer(80);
bool otaAtivo = false;
String otaSSID = "MonzaDash-OTA";
String otaSenha = "12345678";
String otaIP = "";
String otaStatusMsg = "Aguardando upload";
int otaPercent = 0;
bool otaUploadEmAndamento = false;
bool otaReiniciarPendente = false;
unsigned long otaReiniciarEm = 0;

// =======================
// SEUS MENUS (MANTIDOS ORIGINAIS)
// =======================
struct Menu {
  const char* titulo;
  const char** itens;
  int total;
  Menu* parent;
};

const char* menuPrincipalItens[] = {"Dashboard","Sensores","Diagnostico","Configuracao"};
const char* submenuSensores[] = {"TPS","MAP","CTS (temp motor)","IAT (temp admissao)","Voltimetro","RPM","Tempo de injecao","CO2 POT","Voltar"};
const char* submenuDiagnostico[] = {"Status ALDL","Codigos ECU","Limpar erros ECU","Status I2C","Status Iluminacao","SD Card","Teste Buzzer","Teste Display","Voltar"};
const char* submenuConfig[] = {"Data e hora","GIF abertura","Alertas","Brilho tela","Dash default","Buzzer","Update via OTA","Voltar"};

Menu menuPrincipal = { "Menu Principal", menuPrincipalItens, 4, nullptr };
Menu menuSensores = { "Sensores", submenuSensores, 9, &menuPrincipal };
Menu menuDiagnostico = { "Diagnostico", submenuDiagnostico, 9, &menuPrincipal };
Menu menuConfig = { "Configuracao", submenuConfig, 8, &menuPrincipal };

Menu* menuAtual = &menuPrincipal;
int menuIndex = 0;

// =======================
// ESTADO DO SISTEMA
// =======================
enum EstadoUI { MENU_UI, TELA_UI };
EstadoUI estadoUI = MENU_UI;
int telaAtiva = 0;
int lastCLK = HIGH;
unsigned long ultimoUpdate = 0;
const unsigned long INTERVALO_UPDATE = 250;
static uint16_t lineBuffer[280];

int leituraIluminacao = 0;
int ajustePasso = 0;

int movimentoEncoder = 0;
bool cliqueDetectado = false;

int modoBuzzer = 1; // 0: Off, 1: Simples, 2: Duplo

unsigned long bytesEnviados = 0;
unsigned long bytesRecebidosTotal = 0;
unsigned long ecosDescartados = 0;
unsigned long contTX = 0;
unsigned long contEco = 0;
unsigned long contErroSync = 0;
unsigned long bytesLixoDescartados = 0;
uint8_t ultimoHeader = 0x00;
uint8_t ultimoID2 = 0x00;

// Variáveis que serão salvas
int brilhoDia = 255;
int brilhoNoite = 255;
String gifAbertura = "abertura.gif";
int freqEncoder = 1200;
int freqClique = 2000;
int freqSucesso = 2500;
int freqErro = 800;

// Variáveis da Multec 700
int valorRPM = 0;
float valorTPS = 0;
float tempMotor = 0;
float valorMAP = 0;
float voltagem = 0;
float voltCO2 = 0;
float tempoInjecao = 0;
float tempAdmissao = 0;
int velocidade = 0;
int errosChecksum = 0;
unsigned long ultimaMensagemALDL = 0;
int pacotesRecebidos = 0;
bool ecuConectada = false;
uint8_t malf1 = 0;
uint8_t malf2 = 0;
uint8_t malf3 = 0;

// Nome arquivo json
const char* configFile = "/config.json";

// ======================================================
// ALDL MULTEC 700
// ======================================================
static const uint32_t ALDL_BAUD_RATE = 8192;
unsigned long aldlPollingMs = 100;
static const unsigned long ALDL_DELAY_SHUTUP_MS = 500;
static const unsigned long ALDL_TIMEOUT_CONECTADA_MS = 2000;

static const uint8_t ALDL_REQ_SHUTUP[] = { 0xF4, 0x56, 0x08, 0xAE };
static const uint8_t ALDL_REQ_STREAM[] = { 0xF4, 0x57, 0x01, 0x00, 0xB4 };
static const uint8_t ALDL_REQ_CLEAR_DTC[] = { 0xF4, 0x57, 0x0A, 0x00, 0xAB };

static const uint8_t ALDL_FRAME_START[] = { 0xF4, 0x95, 0x01 };
static const size_t ALDL_HEADER_LEN = 3;
static const size_t ALDL_PAYLOAD_LEN = 63;
static const size_t ALDL_CHECKSUM_LEN = 1;
static const size_t ALDL_FRAME_LEN = ALDL_HEADER_LEN + ALDL_PAYLOAD_LEN + ALDL_CHECKSUM_LEN;

bool aldlAtivo = false;
bool aldlShutupEnviado = false;
bool aldlBufferLimpoAposShutup = false;
bool aldlPrimeiroFrameOk = false;

unsigned long aldlMillisInicioSessao = 0;
unsigned long aldlUltimoPolling = 0;
const unsigned long ALDL_UI_UPDATE_MS = 180;

uint8_t aldlUltimoTx[8];
size_t aldlUltimoTxLen = 0;

uint8_t aldlRxBuffer[512];
size_t aldlRxLen = 0;

uint8_t aldlUltimoFrame[ALDL_FRAME_LEN];
bool aldlTemFrameValido = false;

// =======================
// PROTÓTIPOS
// =======================
void desenharMenu();
void lerEncoder();
void selecionarItemMenu();
void atualizarTela();
void atualizarDashboard();
void ajustarHora();
void telaStatusI2C();
void telaStatusEletrico();
void telaStatusSD();
void telaStatusALDL();
void telaTesteBuzzer();
void telaTesteDisplay();
void telaAjusteBrilho();
void telaSelecaoGif();
void telaConfigBuzzer();
void telaOTA();
void telaSensorCO2();
void telaCodigosECU();
void telaLimparErrosECU();
std::vector<String> obterFalhasAtivas();
void salvarConfiguracoes();
void carregarConfiguracoes();

void * GIFOpenFile(const char *fname, int32_t *pSize);
void GIFCloseFile(void *pHandle);
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);
void GIFDraw(GIFDRAW *pDraw);
void rodarGifAbertura(String nomeArquivo);

void atualizarBME();
void atualizarMPU();
void ajustarBacklight();
void beep(int freq);

// ALDL
void limparEstadoALDL();
void iniciarALDL();
void pararALDL();
void salvarUltimoTx(const uint8_t* data, size_t len);
void aldlEnviarBytes(const uint8_t* data, size_t len);
void enviarShutupALDL();
void enviarRequisicao();
bool checksumValidoALDL(const uint8_t* frame, size_t len);
int indexOfSequence(const uint8_t* source, size_t sourceLen, const uint8_t* pattern, size_t patternLen);
void removerInicioRx(size_t qtd);
void descartarEcoDoInicioALDL();
uint8_t lerPayloadByte(const uint8_t* frame, size_t start);
void processarDados(const uint8_t* frame);
void processarBufferRecepcaoALDL();
void loopALDL();

// OTA
void iniciarOTA();
void pararOTA();
void loopOTA();
void configurarRotasOTA();

// TELAS
bool telaEhSensorALDL(int tela);
void desenharTituloTelaSensor(const char* titulo);
void desenharRodapeSensor();
void telaSensorTPS();
void telaSensorMAP();
void telaSensorCTS();
void telaSensorIAT();
void telaSensorVoltimetro();
void telaSensorRPM();
void telaSensorTempoInjecao();

// =======================
// SETUP
// =======================
void setup() {
  Serial.begin(115200);

  Serial2.setRxBufferSize(512);

  ledcAttachChannel(TFT_BLK, 5000, 8, 0);
  ledcWrite(TFT_BLK, 255);

  ledcAttachChannel(PIN_BUZZER, 1000, 8, 1);

  SPI.begin(SPI_CLK, -1, SPI_MOSI);
  tft.init(240, 280);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (SD.begin(SD_CS)) {
    carregarConfiguracoes();
    rodarGifAbertura(gifAbertura);
  }

  bme.begin(0x76);
  mpu.begin(0x69);
  rtc.begin();

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  lastCLK = digitalRead(ENC_CLK);

  ajustarBacklight();

  beep(freqSucesso);
  desenharMenu();
}

// =======================
// LOOP PRINCIPAL
// =======================
void loop() {
  if (estadoUI == TELA_UI && telaEhSensorALDL(telaAtiva)) {
    if (!aldlAtivo) {
      iniciarALDL();
    }
    loopALDL();
  } else {
    if (aldlAtivo) {
      pararALDL();
    }
  }

  if (estadoUI == TELA_UI && telaAtiva == TELA_OTA) {
    loopOTA();
  }

  ajustarBacklight();
  lerEncoder();

  if (estadoUI == TELA_UI) {
    unsigned long intervaloAtual = INTERVALO_UPDATE;
    if (telaEhSensorALDL(telaAtiva)) intervaloAtual = ALDL_UI_UPDATE_MS;
    if (telaAtiva == TELA_OTA) intervaloAtual = 250;

    if (millis() - ultimoUpdate >= intervaloAtual || movimentoEncoder != 0) {
      ultimoUpdate = millis();
      atualizarTela();
    }
  }

  delay(1);
}

// =======================
// FUNÇÕES DE MENU E ENCODER
// =======================
void desenharMenu() {
  tft.fillScreen(ST77XX_BLACK);

  int screenW = tft.width();
  int screenH = tft.height();
  int itemHeight = 35;
  int visibleItems = (screenH - 55) / itemHeight;
  int scrollOffset = (menuIndex >= visibleItems) ? menuIndex - visibleItems + 1 : 0;

  tft.setTextSize(2);
  int16_t x1, y1; uint16_t w, h_titulo;
  tft.getTextBounds(menuAtual->titulo, 0, 0, &x1, &y1, &w, &h_titulo);
  tft.setCursor((screenW - w) / 2, 15);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.print(menuAtual->titulo);

  tft.drawFastHLine(20, 38, screenW - 40, ST77XX_GREY);

  for (int i = 0; i < visibleItems; i++) {
    int itemIndex = i + scrollOffset;
    if (itemIndex >= menuAtual->total) break;

    int y = 55 + i * itemHeight;
    uint16_t corItem = (itemIndex == menuIndex) ? ST77XX_CYAN : ST77XX_WHITE;

    if (itemIndex == menuIndex) {
      tft.drawRoundRect(10, y - 4, screenW - 20, 32, 8, ST77XX_CYAN);
    }

    if (menuAtual == &menuPrincipal) {
      const unsigned char* bitmap = nullptr;
      switch(itemIndex) {
        case 0: bitmap = icone_dash; break;
        case 1: bitmap = icone_sensor; break;
        case 2: bitmap = icone_engine; break;
        case 3: bitmap = icone_config; break;
      }

      if (bitmap != nullptr) {
        tft.drawBitmap(20, y, bitmap, 24, 24, corItem);
      }

      tft.setCursor(55, y + 4);
    } else {
      tft.setCursor(30, y + 4);
    }

    tft.setTextColor(corItem, ST77XX_BLACK);
    tft.print(menuAtual->itens[itemIndex]);
  }
}

void lerEncoder() {
  movimentoEncoder = 0;
  cliqueDetectado = false;

  if (digitalRead(ENC_SW) == LOW) {
    delay(50);
    cliqueDetectado = true;

    if (estadoUI == TELA_UI &&
      telaAtiva != TELA_AJUSTAR_HORA &&
      telaAtiva != TELA_AJUSTE_BRILHO &&
      telaAtiva != TELA_CONFIG_BUZZER &&
      telaAtiva != TELA_SELECIONAR_GIF &&
      telaAtiva != TELA_ALDL &&
      telaAtiva != TELA_TPS &&
      telaAtiva != TELA_MAP &&
      telaAtiva != TELA_CTS &&
      telaAtiva != TELA_IAT &&
      telaAtiva != TELA_VOLT &&
      telaAtiva != TELA_RPM &&
      telaAtiva != TELA_TEMPO_INJECAO &&
      telaAtiva != TELA_OTA &&
      telaAtiva != TELA_CO2 &&
      telaAtiva != TELA_CODIGOS_ECU &&
      telaAtiva != TELA_LIMPAR_ECU) {

      estadoUI = MENU_UI;
      desenharMenu();
      beep(freqClique);
      cliqueDetectado = false;
    }
    else if (estadoUI == MENU_UI) {
      selecionarItemMenu();
      cliqueDetectado = false;
    }

    while (digitalRead(ENC_SW) == LOW);
  }

  int clk = digitalRead(ENC_CLK);
  if (clk != lastCLK) {
    if (clk == LOW) {
      movimentoEncoder = (digitalRead(ENC_DT) != clk) ? 1 : -1;
      if (estadoUI == MENU_UI) {
        menuIndex = (menuIndex + movimentoEncoder + menuAtual->total) % menuAtual->total;
        desenharMenu();
        beep(1200);
      }
    }
    lastCLK = clk;
  }
}

void selecionarItemMenu() {
  if (strcmp(menuAtual->itens[menuIndex], "Voltar") == 0) {
    if (menuAtual->parent != nullptr) {
      menuAtual = menuAtual->parent;
      menuIndex = 0;
    }
    desenharMenu();
    return;
  }

  if (menuAtual == &menuPrincipal) {
    if (menuIndex == 0)      { telaAtiva = TELA_DASHBOARD;  estadoUI = TELA_UI; }
    else if (menuIndex == 1) { menuAtual = &menuSensores;    menuIndex = 0; }
    else if (menuIndex == 2) { menuAtual = &menuDiagnostico; menuIndex = 0; }
    else if (menuIndex == 3) { menuAtual = &menuConfig;      menuIndex = 0; }
  }

  else if (menuAtual == &menuSensores) {
    if (menuIndex == 0)      { telaAtiva = TELA_TPS; estadoUI = TELA_UI; }
    else if (menuIndex == 1) { telaAtiva = TELA_MAP; estadoUI = TELA_UI; }
    else if (menuIndex == 2) { telaAtiva = TELA_CTS; estadoUI = TELA_UI; }
    else if (menuIndex == 3) { telaAtiva = TELA_IAT; estadoUI = TELA_UI; }
    else if (menuIndex == 4) { telaAtiva = TELA_VOLT; estadoUI = TELA_UI; }
    else if (menuIndex == 5) { telaAtiva = TELA_RPM; estadoUI = TELA_UI; }
    else if (menuIndex == 6) { telaAtiva = TELA_TEMPO_INJECAO; estadoUI = TELA_UI; }
    else if (menuIndex == 7) { telaAtiva = TELA_CO2; estadoUI = TELA_UI; }
  }

  else if (menuAtual == &menuDiagnostico) {
    if (menuIndex == 0)      { telaAtiva = TELA_ALDL;          estadoUI = TELA_UI; }
    else if (menuIndex == 1) { telaAtiva = TELA_CODIGOS_ECU;   estadoUI = TELA_UI; }
    else if (menuIndex == 2) { telaAtiva = TELA_LIMPAR_ECU;    estadoUI = TELA_UI; }
    else if (menuIndex == 3) { telaAtiva = TELA_TESTE_I2C;     estadoUI = TELA_UI; }
    else if (menuIndex == 4) { telaAtiva = TELA_TESTE_ELET;    estadoUI = TELA_UI; }
    else if (menuIndex == 5) { telaAtiva = TELA_STATUS_SD;     estadoUI = TELA_UI; }
    else if (menuIndex == 6) { telaAtiva = TELA_TESTE_BUZZER;  estadoUI = TELA_UI; }
    else if (menuIndex == 7) { telaAtiva = TELA_TESTE_DISPLAY; estadoUI = TELA_UI; }
  }

  else if (menuAtual == &menuConfig) {
    if (menuIndex == 0) { telaAtiva = TELA_AJUSTAR_HORA; estadoUI = TELA_UI; }
    else if (menuIndex == 1) { telaAtiva = TELA_SELECIONAR_GIF; estadoUI = TELA_UI; }
    else if (menuIndex == 3) { telaAtiva = TELA_AJUSTE_BRILHO; estadoUI = TELA_UI; }
    else if (menuIndex == 5) { telaAtiva = TELA_CONFIG_BUZZER; estadoUI = TELA_UI; }
    else if (menuIndex == 6) { telaAtiva = TELA_OTA; estadoUI = TELA_UI; }
  }

  if (estadoUI == MENU_UI) {
    desenharMenu();
  } else {
    tft.fillScreen(ST77XX_BLACK);

    

    if (telaAtiva == TELA_OTA) {
      iniciarOTA();
    }
  }

  beep(freqClique);
}

// =======================
// RENDERIZAÇÃO DE TELAS
// =======================
void atualizarTela() {
  switch (telaAtiva) {
    case TELA_DASHBOARD:    atualizarDashboard(); break;
    case TELA_AJUSTAR_HORA: ajustarHora(); break;
    case TELA_ALDL:         telaStatusALDL(); break;
    case TELA_TESTE_I2C:    telaStatusI2C(); break;
    case TELA_TESTE_ELET:   telaStatusEletrico(); break;
    case TELA_STATUS_SD:    telaStatusSD(); break;
    case TELA_TESTE_BUZZER: telaTesteBuzzer(); break;
    case TELA_TESTE_DISPLAY: telaTesteDisplay(); break;
    case TELA_AJUSTE_BRILHO: telaAjusteBrilho(); break;
    case TELA_CONFIG_BUZZER: telaConfigBuzzer(); break;
    case TELA_SELECIONAR_GIF: telaSelecaoGif(); break;
    case TELA_BME:          atualizarBME(); break;
    case TELA_MPU:          atualizarMPU(); break;
    case TELA_OTA:          telaOTA(); break;
    case TELA_TPS:            telaSensorTPS(); break;
    case TELA_MAP:            telaSensorMAP(); break;
    case TELA_CTS:            telaSensorCTS(); break;
    case TELA_IAT:            telaSensorIAT(); break;
    case TELA_VOLT:           telaSensorVoltimetro(); break;
    case TELA_RPM:            telaSensorRPM(); break;
    case TELA_TEMPO_INJECAO:  telaSensorTempoInjecao(); break;
    case TELA_CO2:           telaSensorCO2(); break;
    case TELA_CODIGOS_ECU:    telaCodigosECU(); break;
    case TELA_LIMPAR_ECU: telaLimparErrosECU(); break;
  }
}

void desenharTituloTelaSensor(const char* titulo) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(titulo, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((280 - w) / 2, 12);
  tft.print(titulo);
  tft.drawFastHLine(30, 32, 220, ST77XX_GREY);
}

void desenharRodapeSensor() {
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
  tft.setCursor(55, 240);
  tft.print("Clique para voltar");
}

void telaSensorTPS() {
  static bool iniciado = false;
  static float ultimoValor = -9999;

  if (!iniciado) {
    desenharTituloTelaSensor("TPS");
    desenharRodapeSensor();
    iniciado = true;
  }

  if (ultimoValor != valorTPS) {
    tft.fillRect(0, 70, 280, 120, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(80, 70);
    tft.print("ABERTURA");

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(55, 115);
    tft.printf("%.1f%%", valorTPS);

    ultimoValor = valorTPS;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoValor = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaSensorMAP() {
  static bool iniciado = false;
  static float ultimoValor = -9999;

  if (!iniciado) {
    desenharTituloTelaSensor("MAP");
    desenharRodapeSensor();
    iniciado = true;
  }

  float valorTela = round(valorMAP * 100.0f) / 100.0f;

  if (ultimoValor != valorTela) {
    tft.fillRect(0, 70, 280, 120, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(60, 70);
    tft.print("MAP SENSOR");

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(45, 115);
    tft.printf("%.2f", valorTela);

    tft.setTextSize(2);
    tft.setCursor(105, 175);
    tft.print("V");

    ultimoValor = valorTela;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoValor = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaSensorCTS() {
  static bool iniciado = false;
  static float ultimoValor = -9999;

  if (!iniciado) {
    desenharTituloTelaSensor("CTS");
    desenharRodapeSensor();
    iniciado = true;
  }

  if (ultimoValor != tempMotor) {
    tft.fillRect(0, 70, 280, 120, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(35, 70);
    tft.print("TEMP. DO MOTOR");

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(50, 115);
    tft.printf("%.1f", tempMotor);

    tft.setTextSize(2);
    tft.setCursor(170, 175);
    tft.print("C");

    ultimoValor = tempMotor;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoValor = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaSensorIAT() {
  static bool iniciado = false;
  static float ultimoValor = -9999;

  if (!iniciado) {
    desenharTituloTelaSensor("IAT");
    desenharRodapeSensor();
    iniciado = true;
  }

  if (ultimoValor != tempAdmissao) {
    tft.fillRect(0, 70, 280, 120, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(20, 70);
    tft.print("TEMP. ADMISSAO");

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(50, 115);
    tft.printf("%.1f", tempAdmissao);

    tft.setTextSize(2);
    tft.setCursor(170, 175);
    tft.print("C");

    ultimoValor = tempAdmissao;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoValor = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaSensorVoltimetro() {
  static bool iniciado = false;
  static float ultimoValor = -9999;

  if (!iniciado) {
    desenharTituloTelaSensor("VOLTIMETRO");
    desenharRodapeSensor();
    iniciado = true;
  }

  if (ultimoValor != voltagem) {
    tft.fillRect(0, 70, 280, 120, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(65, 70);
    tft.print("BATERIA");

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(45, 115);
    tft.printf("%.1f", voltagem);

    tft.setTextSize(2);
    tft.setCursor(175, 175);
    tft.print("V");

    ultimoValor = voltagem;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoValor = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaSensorRPM() {
  static bool iniciado = false;
  static int ultimoValor = -99999;

  if (!iniciado) {
    desenharTituloTelaSensor("RPM");
    desenharRodapeSensor();
    iniciado = true;
  }

  if (ultimoValor != valorRPM) {
    tft.fillRect(0, 70, 280, 120, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(95, 70);
    tft.print("GIRO");

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(35, 115);
    tft.print(valorRPM);

    ultimoValor = valorRPM;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoValor = -99999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaSensorTempoInjecao() {
  static bool iniciado = false;
  static float ultimoValor = -9999;

  if (!iniciado) {
    desenharTituloTelaSensor("TEMPO INJECAO");
    desenharRodapeSensor();
    iniciado = true;
  }

  if (ultimoValor != tempoInjecao) {
    tft.fillRect(0, 70, 280, 120, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 70);
    tft.print("BICO ABERTO");

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(45, 115);
    tft.printf("%.2f", tempoInjecao);

    tft.setTextSize(2);
    tft.setCursor(170, 175);
    tft.print("ms");

    ultimoValor = tempoInjecao;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoValor = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}


void atualizarDashboard() {
  DateTime now = rtc.now();
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;

  int screenW = 280;

  tft.setTextSize(6);
  tft.setTextColor(0x07FF, ST77XX_BLACK);
  tft.setCursor(45, 25);
  tft.printf("%02d:%02d", now.hour(), now.minute());

  tft.setTextSize(2);
  tft.setTextColor(0x03EF, ST77XX_BLACK);
  tft.setCursor(225, 55);
  tft.printf("%02d", now.second());

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(80, 80);
  tft.printf("%02d/%02d/%04d", now.day(), now.month(), now.year());

  tft.drawFastHLine(40, 102, 200, 0x03EF);

  int cardW = 110;
  int cardH = 60;
  int yCards = 115;
  int margemLateral = 25;

  tft.drawRoundRect(margemLateral, yCards, cardW, cardH, 4, 0x07FF);
  tft.setTextColor(0x07FF, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(margemLateral + 10, yCards + 8); tft.print("TEMPERATURE");
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(margemLateral + 10, yCards + 25); tft.printf("%.1f", temp);
  tft.setTextSize(1); tft.print(" oC");

  int xHum = margemLateral + cardW + 10;
  tft.drawRoundRect(xHum, yCards, cardW, cardH, 4, 0x07FF);
  tft.setTextColor(0x07FF, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(xHum + 10, yCards + 8); tft.print("HUMIDITY");
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(xHum + 15, yCards + 25); tft.printf("%.0f", hum);
  tft.setTextSize(2); tft.print("%");

  int xPressao = 40;
  tft.setTextSize(1);
  tft.setTextColor(0x07FF, ST77XX_BLACK);
  tft.setCursor(xPressao, 190); tft.print("ATMOSPHERIC PRESSURE");

  tft.drawRect(xPressao, 202, 200, 10, 0x03EF);
  int larguraBarra = map(constrain(pres, 950, 1050), 950, 1050, 0, 196);
  tft.fillRect(xPressao + 2, 204, larguraBarra, 6, 0x07FF);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(xPressao, 218);
  tft.printf("%.1f hPa", pres);
}

void ajustarHora() {
  static bool iniciado = false;
  static int h, m, d, mes, ano;

  if (!iniciado) {
    DateTime now = rtc.now();
    h = now.hour(); m = now.minute();
    d = now.day(); mes = now.month(); ano = now.year();
    ajustePasso = 0;
    iniciado = true;
    tft.fillScreen(ST77XX_BLACK);
  }

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(25, 20);
  tft.print("CONFIGURAR DATA/HORA");
  tft.drawFastHLine(40, 42, 200, ST77XX_GREY);

  if (movimentoEncoder != 0) {
    if (ajustePasso == 0) h = (h + movimentoEncoder + 24) % 24;
    else if (ajustePasso == 1) m = (m + movimentoEncoder + 60) % 60;
    else if (ajustePasso == 2) { d += movimentoEncoder; if(d<1) d=31; if(d>31) d=1; }
    else if (ajustePasso == 3) { mes += movimentoEncoder; if(mes<1) mes=12; if(mes>12) mes=1; }
    else if (ajustePasso == 4) ano += movimentoEncoder;
    beep(freqEncoder);
    movimentoEncoder = 0;
  }

  if (cliqueDetectado) {
    ajustePasso++;
    beep(freqSucesso);
    cliqueDetectado = false;

    if (ajustePasso > 4) {
      rtc.adjust(DateTime(ano, mes, d, h, m, 0));
      iniciado = false;
      estadoUI = MENU_UI;
      tft.fillScreen(ST77XX_BLACK);
      desenharMenu();
      return;
    }
  }

  tft.setTextSize(6);
  int xRelogio = 55;
  tft.setCursor(xRelogio, 75);

  tft.setTextColor(ajustePasso == 0 ? 0x07FF : ST77XX_WHITE, ST77XX_BLACK);
  if (h < 10) tft.print("0"); tft.print(h);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); tft.print(":");

  tft.setTextColor(ajustePasso == 1 ? 0x07FF : ST77XX_WHITE, ST77XX_BLACK);
  if (m < 10) tft.print("0"); tft.print(m);

  tft.setTextSize(3);
  int xData = 50;
  tft.setCursor(xData, 155);

  tft.setTextColor(ajustePasso == 2 ? 0x07FF : ST77XX_WHITE, ST77XX_BLACK);
  if (d < 10) tft.print("0"); tft.print(d);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); tft.print("/");

  tft.setTextColor(ajustePasso == 3 ? 0x07FF : ST77XX_WHITE, ST77XX_BLACK);
  if (mes < 10) tft.print("0"); tft.print(mes);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); tft.print("/");

  tft.setTextColor(ajustePasso == 4 ? 0x07FF : ST77XX_WHITE, ST77XX_BLACK);
  tft.print(ano);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
  tft.setCursor(25, 215);
  tft.print("Gire para ajustar - Clique para proximo");
}

// ======================================================
// TELAS DE DIAGNÓSTICO
// ======================================================

void telaStatusI2C() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(75, 20);
  tft.print("SCANNER I2C");
  tft.drawFastHLine(40, 42, 200, ST77XX_GREY);

  int encontrados = 0;
  int yPos = 65;
  int xPos = 40;

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      tft.drawBitmap(xPos, yPos, icone_ok, 16, 16, 0x07FF);
      tft.setCursor(xPos + 25, yPos);
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.printf("0x%02X", address);

      if (address == 0x76)      tft.print(" [BME280]");
      else if (address == 0x69) tft.print(" [MPU6050]");
      else if (address == 0x68) tft.print(" [DS3231]");
      else                      tft.printf(" [0x%02X]", address);

      encontrados++;
      yPos += 30;
    }
  }
  if (encontrados == 0) {
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setCursor(50, 100); tft.print("SEM DISPOSITIVOS");
  }
}

void telaStatusEletrico() {
  static bool iniciado = false;
  static int ultimoLdr = -1;
  static int ultimoFarol = -1;

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(45, 20);
    tft.print("MONITOR ELETRICO");
    tft.drawFastHLine(40, 42, 200, ST77XX_GREY);

    iniciado = true;
    ultimoLdr = -1;
    ultimoFarol = -1;
  }

  bool farolOn = (leituraIluminacao > 1500);

  if (ultimoFarol != (int)farolOn) {
    tft.fillRect(40, 95, 200, 30, ST77XX_BLACK);

    tft.drawBitmap(50, 100, farolOn ? icone_ok : icone_erro, 16, 16, farolOn ? ST77XX_GREEN : ST77XX_RED);

    tft.setCursor(75, 100);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print("ILL: ");

    tft.setTextColor(farolOn ? ST77XX_GREEN : ST77XX_RED, ST77XX_BLACK);
    tft.print(farolOn ? "ACESO" : "APAGADO");

    ultimoFarol = farolOn;
  }

  if (ultimoLdr != leituraIluminacao) {
    tft.fillRect(40, 145, 220, 20, ST77XX_BLACK);

    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(50, 150);
    tft.printf("SINAL LDR: %d", leituraIluminacao);

    ultimoLdr = leituraIluminacao;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoLdr = -1;
    ultimoFarol = -1;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaStatusSD() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(85, 20);
  tft.print("CARTAO SD");
  tft.drawFastHLine(40, 42, 200, ST77XX_GREY);

  int xPos = 40;
  bool sdOk = SD.begin(SD_CS);

  if (!sdOk) {
    tft.drawBitmap(xPos, 80, icone_erro, 16, 16, ST77XX_RED);
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setCursor(xPos + 25, 80); tft.print("SD: ERRO/OFF");
  } else {
    tft.drawBitmap(xPos, 70, icone_ok, 16, 16, ST77XX_GREEN);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(xPos + 25, 70); tft.print("SD: PRONTO");

    uint64_t total = SD.totalBytes() / (1024 * 1024);
    uint64_t usado = SD.usedBytes() / (1024 * 1024);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(xPos, 105);
    tft.printf("%lluMB / %lluMB", usado, total);

    int barraW = 200;
    int barraH = 15;
    int barraX = xPos;
    int barraY = 130;

    tft.drawRect(barraX, barraY, barraW, barraH, 0x03EF);

    int preenchimento = (usado * (barraW - 4)) / total;
    if (preenchimento > barraW - 4) preenchimento = barraW - 4;

    tft.fillRect(barraX + 2, barraY + 2, preenchimento, barraH - 4, 0x07FF);

    tft.setCursor(xPos, 165);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print("LOG: ");
    tft.setTextColor(0x07FF, ST77XX_BLACK); tft.print("GRAVANDO");
  }
}

bool telaEhSensorALDL(int tela) {
  return (
    tela == TELA_ALDL ||
    tela == TELA_TPS ||
    tela == TELA_MAP ||
    tela == TELA_CTS ||
    tela == TELA_IAT ||
    tela == TELA_VOLT ||
    tela == TELA_RPM ||
    tela == TELA_TEMPO_INJECAO ||
    tela == TELA_CO2 ||
    tela == TELA_CODIGOS_ECU ||
    tela == TELA_LIMPAR_ECU
  );
}

void telaStatusALDL() {
  static bool iniciado = false;
  static uint8_t prevHeader = 0xFF;
  static uint8_t prevID2 = 0xFF;
  static bool prevEcuConectada = false;
  static unsigned long prevContTX = 0xFFFFFFFF;
  static unsigned long prevContEco = 0xFFFFFFFF;
  static unsigned long prevPacotes = 0xFFFFFFFF;
  static unsigned long prevLixo = 0xFFFFFFFF;
  static int prevChk = -1;
  static int prevBuf = -1;
  static int prevRPM = -99999;
  static float prevTemp = -99999;
  static float prevBat = -99999;
  static float prevTPS = -99999;
  static float prevMAP = -99999;
  static int prevVel = -99999;
  static unsigned long prevPolling = 0xFFFFFFFF;

  ecuConectada = (millis() - ultimaMensagemALDL < ALDL_TIMEOUT_CONECTADA_MS);

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(65, 10);
    tft.print("DIAGNOSTICO");
    tft.drawFastHLine(30, 30, 220, ST77XX_WHITE);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(25, 240);
    tft.print("GIRE: polling | CLIQUE: voltar");

    iniciado = true;

    prevHeader = 0xFF;
    prevID2 = 0xFF;
    prevEcuConectada = !ecuConectada;
    prevContTX = 0xFFFFFFFF;
    prevContEco = 0xFFFFFFFF;
    prevPacotes = 0xFFFFFFFF;
    prevLixo = 0xFFFFFFFF;
    prevChk = -1;
    prevBuf = -1;
    prevRPM = -99999;
    prevTemp = -99999;
    prevBat = -99999;
    prevTPS = -99999;
    prevMAP = -99999;
    prevVel = -99999;
    prevPolling = 0xFFFFFFFF;
  }

  if (movimentoEncoder != 0) {
    long novoPolling = (long)aldlPollingMs + (movimentoEncoder * 10);
    if (novoPolling < 50) novoPolling = 50;
    if (novoPolling > 1000) novoPolling = 1000;
    aldlPollingMs = (unsigned long)novoPolling;
    movimentoEncoder = 0;
    beep(freqEncoder);
  }

  int xInfo = 15;

  if (prevHeader != ultimoHeader || prevID2 != ultimoID2) {
    tft.fillRect(0, 38, 280, 12, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(xInfo, 40);
    tft.printf("HEADER:0x%02X ID2:0x%02X BAUD:%lu", ultimoHeader, ultimoID2, (unsigned long)ALDL_BAUD_RATE);
    prevHeader = ultimoHeader;
    prevID2 = ultimoID2;
  }

  if (prevContTX != contTX || prevContEco != contEco || prevPacotes != (unsigned long)pacotesRecebidos) {
    tft.fillRect(0, 58, 280, 20, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(xInfo, 62);
    tft.printf("TX:%lu ECO:%lu RX:%d", contTX, contEco, pacotesRecebidos);
    prevContTX = contTX;
    prevContEco = contEco;
    prevPacotes = pacotesRecebidos;
  }

  if (prevLixo != bytesLixoDescartados || prevChk != errosChecksum || prevBuf != (int)aldlRxLen) {
    tft.fillRect(0, 85, 280, 18, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);
    tft.setCursor(xInfo, 88);
    tft.printf("LIXO:%lu", bytesLixoDescartados);

    tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
    tft.setCursor(110, 88);
    tft.printf("CHK:%d", errosChecksum);

    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(200, 88);
    tft.printf("BUF:%d", (int)aldlRxLen);

    prevLixo = bytesLixoDescartados;
    prevChk = errosChecksum;
    prevBuf = (int)aldlRxLen;
  }

  if (prevRPM != valorRPM || prevTemp != tempMotor || prevBat != voltagem) {
    tft.fillRect(0, 112, 280, 18, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(xInfo, 115);
    tft.printf("RPM:%d", valorRPM);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(125, 115);
    tft.printf("TMP:%.1fC", tempMotor);

    prevRPM = valorRPM;
    prevTemp = tempMotor;
  }

  if (prevTPS != valorTPS || prevMAP != valorMAP || prevVel != velocidade || prevBat != voltagem) {
    tft.fillRect(0, 142, 280, 40, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(xInfo, 145);
    tft.printf("TPS:%.1f%%", valorTPS);

    tft.setCursor(xInfo, 168);
    tft.printf("MAP:%.2fV", valorMAP);

    tft.setCursor(160, 145);
    tft.printf("BAT:%.1fV", voltagem);

    tft.setCursor(160, 168);
    tft.printf("VEL:%dkm", velocidade);

    prevTPS = valorTPS;
    prevMAP = valorMAP;
    prevVel = velocidade;
    prevBat = voltagem;
  }

  if (prevPolling != aldlPollingMs) {
    tft.fillRect(0, 192, 280, 18, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(xInfo, 195);
    tft.printf("POLLING: %lums", aldlPollingMs);
    prevPolling = aldlPollingMs;
  }

  if (prevEcuConectada != ecuConectada) {
    tft.fillRect(0, 218, 280, 16, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(15, 220);
    if (ecuConectada) {
      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      tft.print("ECU ONLINE");
    } else {
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft.print("ECU OFFLINE");
    }
    prevEcuConectada = ecuConectada;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    salvarConfiguracoes();
    pararALDL();
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaTesteBuzzer() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(65, 20);
  tft.print("TESTE DE AUDIO");
  tft.drawFastHLine(40, 42, 200, ST77XX_GREY);

  int xPos = 40;

  tft.setCursor(xPos, 80);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("STATUS: ");
  tft.setTextColor(0x07FF, ST77XX_BLACK);
  tft.print("PRONTO");

  tft.setCursor(xPos, 120);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("GIRE: Testar Tons");

  tft.setCursor(xPos, 160);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("CLIQUE: Bip Curto");

  if (movimentoEncoder != 0) {
    static int freqTeste = 1500;
    freqTeste += (movimentoEncoder * 100);
    if(freqTeste < 500) freqTeste = 500;
    if(freqTeste > 4000) freqTeste = 4000;

    ledcWriteTone(PIN_BUZZER, freqTeste);
    delay(50);
    ledcWriteTone(PIN_BUZZER, 0);

    tft.fillRect(xPos, 190, 200, 20, ST77XX_BLACK);
    tft.setCursor(xPos, 190);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.printf("%d Hz", freqTeste);

    movimentoEncoder = 0;
  }

  if (cliqueDetectado) {
    beep(freqSucesso);
    cliqueDetectado = false;
  }
}

void telaTesteDisplay() {
  static int corTeste = 0;
  static int ultimaCor = -1;

  uint16_t cores[] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, ST77XX_WHITE, ST77XX_GREY, ST77XX_YELLOW};
  const char* nomes[] = {"VERMELHO", "VERDE", "AZUL", "BRANCO", "CINZA", "AMARELO"};

  if (movimentoEncoder != 0) {
    corTeste = (corTeste + movimentoEncoder + 6) % 6;
    movimentoEncoder = 0;
  }

  if (corTeste != ultimaCor) {
    tft.fillScreen(cores[corTeste]);
    tft.fillRect(0, 0, 280, 50, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(55, 15);
    tft.printf("COR: %s", nomes[corTeste]);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(55, 35);
    tft.print("GIRE PARA TROCAR / CLIQUE SAIR");

    ultimaCor = corTeste;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    ultimaCor = -1;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaSensorCO2() {
  static bool iniciado = false;
  static float ultimoValor = -9999;

  if (!iniciado) {
    desenharTituloTelaSensor("CO2 POT");
    desenharRodapeSensor();
    iniciado = true;
  }

  if (ultimoValor != voltCO2) {
    tft.fillRect(0, 60, 280, 140, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(70, 65);
    tft.print("AJUSTE CO2");

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(45, 105);
    tft.printf("%.2f", voltCO2);

    tft.setTextSize(2);
    tft.setCursor(180, 165);
    tft.print("V");

    ultimoValor = voltCO2;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoValor = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaCodigosECU() {
  static bool iniciado = false;
  static String cacheFalhas = "";

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(60, 15);
    tft.print("CODIGOS ECU");
    tft.drawFastHLine(30, 35, 220, ST77XX_GREY);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(55, 240);
    tft.print("Clique para voltar");

    iniciado = true;
    cacheFalhas = "__FORCAR__";
  }

  std::vector<String> falhas = obterFalhasAtivas();

  String novoCache = "";
  for (size_t i = 0; i < falhas.size(); i++) {
    novoCache += falhas[i] + "|";
  }
  if (falhas.empty()) {
    novoCache = "SEM_FALHAS";
  }

  if (novoCache != cacheFalhas) {
    tft.fillRect(0, 50, 280, 180, ST77XX_BLACK);

    if (falhas.empty()) {
      tft.drawRoundRect(35, 90, 210, 55, 8, ST77XX_GREEN);
      tft.setTextSize(3);
      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      tft.setCursor(48, 108);
      tft.print("SEM FALHAS");
    } else {
      int y = 55;
      tft.setTextSize(1);

      for (size_t i = 0; i < falhas.size(); i++) {
        if (y > 210) break;

        tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
        tft.setCursor(10, y);
        tft.print("- ");
        tft.print(falhas[i]);
        y += 18;
      }
    }

    cacheFalhas = novoCache;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    cacheFalhas = "";
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

void telaLimparErrosECU() {
  static bool iniciado = false;
  static int opcao = 0; // 0 = LIMPAR, 1 = VOLTAR
  static int ultimaOpcao = -1;
  static bool executando = false;
  static bool mostrouResultado = false;
  static unsigned long momentoEnvio = 0;

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(35, 15);
    tft.print("LIMPAR ERROS ECU");
    tft.drawFastHLine(30, 35, 220, ST77XX_GREY);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);
    tft.setCursor(20, 55);
    tft.print("Use com ignicao ligada.");
    tft.setCursor(20, 70);
    tft.print("Confira depois em Codigos ECU.");

    iniciado = true;
    opcao = 0;
    ultimaOpcao = -1;
    executando = false;
    mostrouResultado = false;
    momentoEnvio = 0;
  }

  if (!executando && !mostrouResultado && movimentoEncoder != 0) {
    opcao += movimentoEncoder;
    if (opcao < 0) opcao = 1;
    if (opcao > 1) opcao = 0;
    movimentoEncoder = 0;
    beep(freqEncoder);
  }

  if (ultimaOpcao != opcao && !executando && !mostrouResultado) {
    tft.fillRect(20, 95, 240, 90, ST77XX_BLACK);

    tft.setTextSize(2);

    if (opcao == 0) {
      tft.drawRoundRect(30, 100, 220, 28, 6, ST77XX_CYAN);
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.setCursor(95, 107);
      tft.print("LIMPAR");

      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(95, 147);
      tft.print("VOLTAR");
    } else {
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(95, 107);
      tft.print("LIMPAR");

      tft.drawRoundRect(30, 140, 220, 28, 6, ST77XX_CYAN);
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.setCursor(95, 147);
      tft.print("VOLTAR");
    }

    ultimaOpcao = opcao;
  }

  if (cliqueDetectado && !executando && !mostrouResultado) {
    cliqueDetectado = false;

    if (opcao == 0) {
      enviarLimparErrosECU();
      executando = true;
      momentoEnvio = millis();

      tft.fillRect(20, 95, 240, 90, ST77XX_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.setCursor(45, 120);
      tft.print("LIMPANDO ERROS...");
    } else {
      iniciado = false;
      ultimaOpcao = -1;
      estadoUI = MENU_UI;
      tft.fillScreen(ST77XX_BLACK);
      desenharMenu();
      return;
    }
  }

  if (executando && (millis() - momentoEnvio > 500)) {
    while (Serial2.available()) {
      Serial2.read();
    }
    aldlRxLen = 0;

    tft.fillRect(20, 95, 240, 90, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(35, 110);
    tft.print("COMANDO ENVIADO");

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(55, 145);
    tft.print("Clique para sair");

    executando = false;
    mostrouResultado = true;
  }

  if (cliqueDetectado && mostrouResultado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimaOpcao = -1;
    executando = false;
    mostrouResultado = false;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

std::vector<String> obterFalhasAtivas() {
  std::vector<String> falhas;

  // MALF1
  if (malf1 & (1 << 0)) falhas.push_back("ERRO 24 - Sem sinal sensor velocidade");
  if (malf1 & (1 << 2)) falhas.push_back("ERRO 22 - TPS voltagem baixa");
  if (malf1 & (1 << 3)) falhas.push_back("ERRO 21 - TPS voltagem alta");
  if (malf1 & (1 << 4)) falhas.push_back("ERRO 15 - Sensor temperatura / chicote");
  if (malf1 & (1 << 5)) falhas.push_back("ERRO 14 - Sensor temperatura / curto massa");
  if (malf1 & (1 << 7)) falhas.push_back("ERRO 12 - Sem pulsos referencia");

  // MALF2
  if (malf2 & (1 << 0)) falhas.push_back("ERRO 42 - Modulo HEI");
  if (malf2 & (1 << 2)) falhas.push_back("ERRO 35 - Atuador IAC");
  if (malf2 & (1 << 3)) falhas.push_back("ERRO 34 - MAP sinal baixo");
  if (malf2 & (1 << 4)) falhas.push_back("ERRO 33 - MAP sinal alto");
  if (malf2 & (1 << 5)) falhas.push_back("ERRO 32 - Falha sistema EGR");

  // MALF3
  if (malf3 & (1 << 0)) falhas.push_back("ERRO 55 - Unidade comando defeito");
  if (malf3 & (1 << 1)) falhas.push_back("ERRO 54 - Potenciometro CO2");
  if (malf3 & (1 << 4)) falhas.push_back("ERRO 51 - Unidade comando / MEM-CAL");

  return falhas;
}

// =======================
// TELA OTA
// =======================
void telaOTA() {
  static bool iniciado = false;
  static String prevStatus = "";
  static int prevPercent = -1;
  static String prevIP = "";
  static bool prevAtivo = false;

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(70, 15);
    tft.print("UPDATE VIA OTA");
    tft.drawFastHLine(30, 35, 220, ST77XX_GREY);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(20, 245);
    tft.print("Conecte no AP e abra 192.168.4.1");

    iniciado = true;
    prevStatus = "";
    prevPercent = -1;
    prevIP = "";
    prevAtivo = !otaAtivo;
  }

  if (prevAtivo != otaAtivo) {
    tft.fillRect(0, 50, 280, 14, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 50);
    if (otaAtivo) {
      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      tft.print("AP ATIVO");
    } else {
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft.print("AP INATIVO");
    }
    prevAtivo = otaAtivo;
  }

  if (prevIP != otaIP) {
    tft.fillRect(0, 80, 280, 40, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(20, 80);
    tft.print("SSID:");
    tft.setCursor(20, 100);
    tft.print("IP:");
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(80, 80);
    tft.print(otaSSID);
    tft.setCursor(60, 100);
    tft.print(otaIP);
    prevIP = otaIP;
  }

  if (prevStatus != otaStatusMsg) {
    tft.fillRect(0, 130, 280, 40, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(20, 130);
    tft.print("STATUS:");
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(20, 150);
    tft.print(otaStatusMsg);
    prevStatus = otaStatusMsg;
  }

  if (prevPercent != otaPercent) {
    tft.fillRect(20, 185, 240, 30, ST77XX_BLACK);
    tft.drawRect(20, 185, 220, 16, ST77XX_CYAN);
    int largura = map(otaPercent, 0, 100, 0, 216);
    if (largura > 0) {
      tft.fillRect(22, 187, largura, 12, ST77XX_CYAN);
    }

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(20, 205);
    tft.printf("%d%%", otaPercent);
    prevPercent = otaPercent;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    prevStatus = "";
    prevPercent = -1;
    prevIP = "";
    prevAtivo = false;
    pararOTA();
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
  }
}

// ======================================================
// TELAS DE CONFIGURAÇÃO
// ======================================================

void telaAjusteBrilho() {
  static bool iniciado = false;
  static int passoBrilho = 0;

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);
    iniciado = true;
    passoBrilho = 0;
  }

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(60, 20); tft.print("AJUSTE BRILHO");
  tft.drawFastHLine(40, 42, 200, ST77XX_GREY);

  if (movimentoEncoder != 0) {
    if (passoBrilho == 0) {
      brilhoDia = constrain(brilhoDia + (movimentoEncoder * 15), 15, 255);
      ledcWrite(TFT_BLK, brilhoDia);
    } else {
      brilhoNoite = constrain(brilhoNoite + (movimentoEncoder * 15), 5, 255);
      ledcWrite(TFT_BLK, brilhoNoite);
    }
    movimentoEncoder = 0;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    passoBrilho++;
    beep(freqSucesso);

    if (passoBrilho > 1) {
      salvarConfiguracoes();
      iniciado = false;
      estadoUI = MENU_UI;
      tft.fillScreen(ST77XX_BLACK);
      desenharMenu();
      return;
    }
  }

  int xPos = 60;
  tft.setTextColor((passoBrilho == 0) ? ST77XX_CYAN : ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(xPos, 80);
  tft.printf("DIA: %3d%% ", map(brilhoDia, 0, 255, 0, 100));

  tft.setTextColor((passoBrilho == 1) ? ST77XX_CYAN : ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(xPos, 130);
  tft.printf("NOITE: %3d%% ", map(brilhoNoite, 0, 255, 0, 100));
}

void telaSelecaoGif() {
  static bool lido = false;
  static std::vector<String> arquivosGif;
  static int selGif = 0;

  if (!lido) {
    arquivosGif.clear();
    File raiz = SD.open("/");
    File arquivo = raiz.openNextFile();
    while (arquivo) {
      String nome = arquivo.name();
      if (nome.endsWith(".gif") || nome.endsWith(".GIF")) {
        arquivosGif.push_back(nome);
      }
      arquivo = raiz.openNextFile();
    }
    lido = true;
    tft.fillScreen(ST77XX_BLACK);
  }

  tft.setTextSize(2);
  tft.setCursor(45, 20);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.print("ESCOLHER ABERTURA");
  tft.drawFastHLine(40, 42, 200, ST77XX_GREY);

  if (arquivosGif.size() == 0) {
    tft.setCursor(50, 100);
    tft.print("NENHUM .GIF NO SD");
  } else {
    if (movimentoEncoder != 0) {
      selGif = (selGif + movimentoEncoder + arquivosGif.size()) % arquivosGif.size();
      movimentoEncoder = 0;
    }

    for (int i = 0; i < (int)arquivosGif.size(); i++) {
      int yPos = 70 + (i * 30);
      if (i == selGif) {
        tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
        tft.setCursor(40, yPos); tft.print("> ");
      } else {
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setCursor(40, yPos); tft.print("  ");
      }
      tft.print(arquivosGif[i]);
    }
  }

  if (cliqueDetectado) {
    if (arquivosGif.size() > 0) {
      gifAbertura = arquivosGif[selGif];
      salvarConfiguracoes();
      beep(freqSucesso);
    }
    lido = false;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
    cliqueDetectado = false;
  }
}

void salvarConfiguracoes() {
  if (!SD.begin(SD_CS)) return;

  if (SD.exists(configFile)) {
    SD.remove(configFile);
  }

  File file = SD.open(configFile, FILE_WRITE);
  if (!file) return;

  JsonDocument doc;
  doc["brilhoDia"] = brilhoDia;
  doc["brilhoNoite"] = brilhoNoite;
  doc["gif"] = gifAbertura;
  doc["fEnc"] = freqEncoder;
  doc["fCli"] = freqClique;
  doc["fSuc"] = freqSucesso;
  doc["fErr"] = freqErro;
  doc["modoBuzzer"] = modoBuzzer;
  doc["aldlPollingMs"] = aldlPollingMs;

  serializeJsonPretty(doc, file);
  file.flush();
  file.close();
}

void carregarConfiguracoes() {
  if (!SD.exists(configFile)) {
    salvarConfiguracoes();
    return;
  }

  File file = SD.open(configFile);
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);

  if (!error) {
    brilhoDia = doc["brilhoDia"] | 255;
    brilhoNoite = doc["brilhoNoite"] | 80;
    gifAbertura = doc["gif"] | "abertura.gif";
    freqEncoder = doc["fEnc"] | 1200;
    freqClique  = doc["fCli"] | 2000;
    freqSucesso = doc["fSuc"] | 2500;
    freqErro    = doc["fErr"] | 800;
    modoBuzzer = doc["modoBuzzer"] | 1;
    aldlPollingMs = doc["aldlPollingMs"] | 100;
  }
  file.close();
}

// ======================================================
// FUNCOES PARA GIF
// ======================================================

void * GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = SD.open(fname);
  if (!gifFile) return NULL;
  *pSize = gifFile.size();
  return (void *)&gifFile;
}

void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f) f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  File *f = static_cast<File *>(pFile->fHandle);

  int32_t iBytesRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos;

  if (iBytesRead <= 0) return 0;

  iBytesRead = f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();

  return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = f->position();
  return pFile->iPos;
}

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s = pDraw->pPixels;
  uint16_t *palette = pDraw->pPalette;

  int x = pDraw->iX;
  int y = pDraw->iY + pDraw->y;
  int width = pDraw->iWidth;

  if (y >= 240 || x >= 280) return;
  if (width + x > 280)
    width = 280 - x;

  if (pDraw->ucDisposalMethod == 2) {
    for (int i = 0; i < width; i++) {
      if (s[i] == pDraw->ucTransparent)
        s[i] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  for (int i = 0; i < width; i++) {
    uint8_t index = s[i];
    if (pDraw->ucHasTransparency && index == pDraw->ucTransparent)
      continue;
    lineBuffer[i] = palette[index];
  }

  tft.startWrite();
  tft.setAddrWindow(x, y, width, 1);
  tft.writePixels(lineBuffer, width);
  tft.endWrite();
}

void rodarGifAbertura(String nomeArquivo) {
  if (!nomeArquivo.startsWith("/"))
    nomeArquivo = "/" + nomeArquivo;

  if (!SD.exists(nomeArquivo)) {
    return;
  }

  tft.fillScreen(ST77XX_BLACK);

  if (!gif.open(nomeArquivo.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    return;
  }

  unsigned long inicio = millis();

  while (gif.playFrame(true, NULL)) {
    yield();

    if (digitalRead(ENC_SW) == LOW)
      break;

    if (millis() - inicio > 6000)
      break;
  }

  gif.close();
}

void telaConfigBuzzer() {
  static bool iniciado = false;
  static int passo = 0;
  static int itemSel = 0;
  static int ultimoItemSel = -1;
  static int ultimoPasso = -1;

  const char* nomesModo[] = {"MUDO", "SIMPLES", "DUPLO"};
  bool forcaRedesenho = false;

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(60, 20); tft.print("AJUSTE AUDIO");
    tft.drawFastHLine(40, 42, 200, ST77XX_GREY);
    iniciado = true;
    forcaRedesenho = true;
  }

  if (movimentoEncoder != 0 || cliqueDetectado || forcaRedesenho) {
    if (movimentoEncoder != 0) {
      if (passo == 0) {
        itemSel = (itemSel + movimentoEncoder + 6) % 6;
      } else {
        if (itemSel == 0) {
          modoBuzzer = (modoBuzzer + movimentoEncoder + 3) % 3;
        } else if (itemSel >= 1 && itemSel <= 4) {
          int* vals[] = {&freqEncoder, &freqClique, &freqSucesso, &freqErro};
          *vals[itemSel - 1] = constrain(*vals[itemSel - 1] + (movimentoEncoder * 100), 400, 4000);
          if (modoBuzzer > 0) {
            ledcWriteTone(PIN_BUZZER, *vals[itemSel - 1]);
            delay(40); ledcWriteTone(PIN_BUZZER, 0);
          }
        }
      }
      movimentoEncoder = 0;
    }

    if (cliqueDetectado) {
      cliqueDetectado = false;
      if (itemSel == 5) {
        salvarConfiguracoes();
        iniciado = false; ultimoItemSel = -1; ultimoPasso = -1;
        estadoUI = MENU_UI;
        tft.fillScreen(ST77XX_BLACK); desenharMenu(); return;
      }
      passo = !passo;
      if (passo == 0) salvarConfiguracoes();
      beep(freqClique);
    }

    for (int i = 0; i < 6; i++) {
      int yPos = 60 + (i * 30);

      if (i == itemSel || i == ultimoItemSel || forcaRedesenho) {
        tft.fillRect(15, yPos - 8, 250, 28, ST77XX_BLACK);

        if (i == itemSel) {
          uint16_t cor = (passo == 1) ? ST77XX_CYAN : ST77XX_WHITE;
          tft.drawRoundRect(15, yPos - 8, 250, 28, 5, cor);
          tft.setTextColor(cor, ST77XX_BLACK);
        } else {
          tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
        }

        tft.setCursor(30, yPos);
        if (i == 0) tft.printf("MODO: %s", nomesModo[modoBuzzer]);
        else if (i == 1) tft.printf("Encoder: %dHz", freqEncoder);
        else if (i == 2) tft.printf("Clique: %dHz", freqClique);
        else if (i == 3) tft.printf("Sucesso: %dHz", freqSucesso);
        else if (i == 4) tft.printf("Erro: %dHz", freqErro);
        else tft.print("Voltar");
      }
    }
    ultimoItemSel = itemSel;
    ultimoPasso = passo;
  }
}

// ======================================================
// ALDL - FUNÇÕES
// ======================================================

void limparEstadoALDL() {
  aldlShutupEnviado = false;
  aldlBufferLimpoAposShutup = false;
  aldlPrimeiroFrameOk = false;
  aldlMillisInicioSessao = 0;
  aldlUltimoPolling = 0;

  aldlUltimoTxLen = 0;
  aldlRxLen = 0;
  aldlTemFrameValido = false;

  bytesEnviados = 0;
  bytesRecebidosTotal = 0;
  ecosDescartados = 0;
  contTX = 0;
  contEco = 0;
  contErroSync = 0;
  bytesLixoDescartados = 0;
  ultimoHeader = 0x00;
  ultimoID2 = 0x00;

  valorRPM = 0;
  valorTPS = 0;
  tempMotor = 0;
  valorMAP = 0;
  voltagem = 0;
  tempoInjecao = 0;
  velocidade = 0;
  errosChecksum = 0;
  ultimaMensagemALDL = 0;
  pacotesRecebidos = 0;
  ecuConectada = false;
}

void iniciarALDL() {
  pararALDL();
  limparEstadoALDL();

  Serial2.begin(ALDL_BAUD_RATE, SERIAL_8N1, RX2, TX2, false);
  Serial2.setRxBufferSize(512);

  while (Serial2.available()) Serial2.read();

  aldlAtivo = true;
  aldlMillisInicioSessao = millis();
  enviarShutupALDL();
}

void pararALDL() {
  if (aldlAtivo) {
    Serial2.end();
  }
  aldlAtivo = false;
  limparEstadoALDL();
}

void salvarUltimoTx(const uint8_t* data, size_t len) {
  aldlUltimoTxLen = min(len, sizeof(aldlUltimoTx));
  memcpy(aldlUltimoTx, data, aldlUltimoTxLen);
}

void aldlEnviarBytes(const uint8_t* data, size_t len) {
  if (!aldlAtivo) return;

  salvarUltimoTx(data, len);
  Serial2.write(data, len);
  Serial2.flush();
  bytesEnviados += len;
  contTX++;
}

void enviarShutupALDL() {
  aldlEnviarBytes(ALDL_REQ_SHUTUP, sizeof(ALDL_REQ_SHUTUP));
  aldlShutupEnviado = true;
}

void enviarRequisicao() {
  aldlEnviarBytes(ALDL_REQ_STREAM, sizeof(ALDL_REQ_STREAM));
}

void enviarLimparErrosECU() {
  aldlEnviarBytes(ALDL_REQ_CLEAR_DTC, sizeof(ALDL_REQ_CLEAR_DTC));
}

bool checksumValidoALDL(const uint8_t* frame, size_t len) {
  uint32_t soma = 0;
  for (size_t i = 0; i < len; i++) soma += frame[i];
  return ((soma & 0xFF) == 0);
}

int indexOfSequence(const uint8_t* source, size_t sourceLen, const uint8_t* pattern, size_t patternLen) {
  if (sourceLen < patternLen || patternLen == 0) return -1;

  for (size_t i = 0; i <= sourceLen - patternLen; i++) {
    bool match = true;
    for (size_t j = 0; j < patternLen; j++) {
      if (source[i + j] != pattern[j]) {
        match = false;
        break;
      }
    }
    if (match) return (int)i;
  }

  return -1;
}

void removerInicioRx(size_t qtd) {
  if (qtd == 0 || qtd > aldlRxLen) return;

  size_t restante = aldlRxLen - qtd;
  if (restante > 0) {
    memmove(aldlRxBuffer, aldlRxBuffer + qtd, restante);
  }
  aldlRxLen = restante;
}

void descartarEcoDoInicioALDL() {
  while (aldlUltimoTxLen > 0 && aldlRxLen >= aldlUltimoTxLen) {
    bool igual = true;

    for (size_t i = 0; i < aldlUltimoTxLen; i++) {
      if (aldlRxBuffer[i] != aldlUltimoTx[i]) {
        igual = false;
        break;
      }
    }

    if (!igual) break;

    removerInicioRx(aldlUltimoTxLen);
    contEco++;
    ecosDescartados += aldlUltimoTxLen;
  }
}

uint8_t lerPayloadByte(const uint8_t* frame, size_t start) {
  return frame[ALDL_HEADER_LEN + (start - 1)];
}

void processarDados(const uint8_t* frame) {
  uint8_t malf1Raw = lerPayloadByte(frame, 3);
  uint8_t malf2Raw = lerPayloadByte(frame, 4);
  uint8_t malf3Raw = lerPayloadByte(frame, 5);

  uint8_t tempRaw  = lerPayloadByte(frame, 6);
  uint8_t ptpsRaw  = lerPayloadByte(frame, 9);
  uint8_t rpmRaw   = lerPayloadByte(frame, 10);
  uint8_t velRaw   = lerPayloadByte(frame, 13);
  uint8_t CO2Raw   = lerPayloadByte(frame, 16);
  uint8_t mapRaw  = lerPayloadByte(frame, 26); // ADMAP - tensão do MAP
  uint8_t matRaw = lerPayloadByte(frame, 30);
  uint8_t battRaw  = lerPayloadByte(frame, 32);
  uint8_t bpwMsb   = lerPayloadByte(frame, 36);
  uint8_t bpwLsb   = lerPayloadByte(frame, 37);

  malf1 = malf1Raw;
  malf2 = malf2Raw;
  malf3 = malf3Raw;

  tempMotor = (tempRaw * 0.75f) - 40.0f;
  tempAdmissao = (matRaw * 0.75f) - 40.0f;
  valorTPS = ptpsRaw / 2.55f;
  valorRPM = (int)(rpmRaw * 25.0f);
  velocidade = velRaw;
  voltagem = battRaw / 10.0f;
  voltCO2 = CO2Raw * (5.0f / 255.0f);
  valorMAP = mapRaw * (5.0f / 255.0f);
  tempoInjecao = ((bpwMsb * 256.0f) + bpwLsb) / 65.536f;

  ultimaMensagemALDL = millis();
  pacotesRecebidos++;
  ecuConectada = true;
}

void processarBufferRecepcaoALDL() {
  bool processou;

  do {
    processou = false;

    descartarEcoDoInicioALDL();

    int startIndex = indexOfSequence(aldlRxBuffer, aldlRxLen, ALDL_FRAME_START, sizeof(ALDL_FRAME_START));

    if (startIndex < 0) {
      if (aldlRxLen > 400) {
        aldlRxLen = 0;
        contErroSync++;
      }
      return;
    }

    if (startIndex > 0) {
      bytesLixoDescartados += (unsigned long)startIndex;
      removerInicioRx((size_t)startIndex);
      processou = true;
      continue;
    }

    if (aldlRxLen < ALDL_FRAME_LEN) {
      return;
    }

    uint8_t frame[ALDL_FRAME_LEN];
    memcpy(frame, aldlRxBuffer, ALDL_FRAME_LEN);

    ultimoHeader = frame[0];
    ultimoID2 = frame[1];

    if (!checksumValidoALDL(frame, ALDL_FRAME_LEN)) {
      errosChecksum++;
      removerInicioRx(1);
      processou = true;
      continue;
    }

    memcpy(aldlUltimoFrame, frame, ALDL_FRAME_LEN);
    aldlTemFrameValido = true;
    aldlPrimeiroFrameOk = true;

    processarDados(frame);
    removerInicioRx(ALDL_FRAME_LEN);
    processou = true;

  } while (processou);
}

void loopALDL() {
  if (!aldlAtivo) return;

  while (Serial2.available() > 0) {
    int b = Serial2.read();
    if (b < 0) break;

    if (aldlRxLen < sizeof(aldlRxBuffer)) {
      aldlRxBuffer[aldlRxLen++] = (uint8_t)b;
      bytesRecebidosTotal++;
    } else {
      aldlRxLen = 0;
      contErroSync++;
      break;
    }
  }

  if (aldlRxLen > 0) {
    processarBufferRecepcaoALDL();
  }

  if (aldlShutupEnviado && millis() - aldlMillisInicioSessao >= ALDL_DELAY_SHUTUP_MS) {
    if (!aldlBufferLimpoAposShutup) {
      while (Serial2.available()) Serial2.read();
      aldlRxLen = 0;
      aldlBufferLimpoAposShutup = true;
    }

    if (millis() - aldlUltimoPolling >= aldlPollingMs) {
      aldlUltimoPolling = millis();
      enviarRequisicao();
    }
  }

  ecuConectada = (millis() - ultimaMensagemALDL < ALDL_TIMEOUT_CONECTADA_MS);
}

// ======================================================
// OTA - FUNÇÕES
// ======================================================

void configurarRotasOTA() {
  otaServer.on("/", HTTP_GET, []() {
    String html;
    html += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Monza OTA</title></head><body style='font-family:Arial;background:#111;color:#eee;padding:20px'>";
    html += "<h2>Monza Dash - OTA</h2>";
    html += "<p>Selecione o arquivo .bin e envie para atualizar.</p>";
    html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='update' accept='.bin'><br><br>";
    html += "<input type='submit' value='Atualizar'>";
    html += "</form>";
    html += "<p>Status atual: " + otaStatusMsg + "</p>";
    html += "</body></html>";
    otaServer.send(200, "text/html", html);
  });

  otaServer.on("/update", HTTP_POST,
    []() {
      bool ok = !Update.hasError();
      otaServer.sendHeader("Connection", "close");
      otaServer.send(200, "text/plain", ok ? "Atualizacao concluida. Reiniciando..." : "Falha na atualizacao");
      if (ok) {
        otaStatusMsg = "Atualizado com sucesso";
        otaPercent = 100;
        otaReiniciarPendente = true;
        otaReiniciarEm = millis() + 2000;
      } else {
        otaStatusMsg = "Erro na atualizacao";
      }
    },
    []() {
      HTTPUpload& upload = otaServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        otaUploadEmAndamento = true;
        otaPercent = 0;
        otaStatusMsg = "Iniciando...";
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
          otaStatusMsg = "Erro ao iniciar";
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
          otaStatusMsg = "Erro escrevendo";
        } else {
          otaPercent = (upload.totalSize > 0) ? (int)((upload.currentSize * 100) / upload.totalSize) : otaPercent;
          otaStatusMsg = "Gravando firmware";
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          otaPercent = 100;
          otaStatusMsg = "Upload finalizado";
        } else {
          Update.printError(Serial);
          otaStatusMsg = "Erro ao finalizar";
        }
        otaUploadEmAndamento = false;
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        otaUploadEmAndamento = false;
        otaStatusMsg = "Upload cancelado";
      }
    }
  );
}

void iniciarOTA() {
  pararOTA();

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(otaSSID.c_str(), otaSenha.c_str());

  if (ok) {
    IPAddress ip = WiFi.softAPIP();
    otaIP = ip.toString();
    otaStatusMsg = "AP iniciado";
    otaPercent = 0;

    configurarRotasOTA();
    otaServer.begin();
    otaAtivo = true;
  } else {
    otaIP = "0.0.0.0";
    otaStatusMsg = "Falha ao iniciar AP";
    otaAtivo = false;
  }
}

void pararOTA() {
  if (otaAtivo) {
    otaServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  otaAtivo = false;
  otaUploadEmAndamento = false;
  otaReiniciarPendente = false;
  otaStatusMsg = "Aguardando upload";
  otaPercent = 0;
  otaIP = "";
}

void loopOTA() {
  if (!otaAtivo) return;

  otaServer.handleClient();

  if (otaReiniciarPendente && millis() >= otaReiniciarEm) {
    ESP.restart();
  }
}

// ======================================================
// OUTRAS FUNCOES
// ======================================================

void atualizarBME() {
  // Limpa somente a área de conteúdo, preservando header/menu se existir
  tft.fillRect(0, 45, 280, 195, ST77XX_BLACK);

  float temperatura = bme.readTemperature();
  float umidade = bme.readHumidity();
  float pressao = bme.readPressure() / 100.0F; // hPa
  float altitude = bme.readAltitude(1013.25);  // referência nível do mar

  tft.setTextWrap(false);

  // Título da tela
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(15, 55);
  tft.print("Sensor BME280");

  // Linha separadora
  tft.drawFastHLine(15, 80, 250, ST77XX_BLUE);

  // Temperatura
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(15, 95);
  tft.print("Temp:");

  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(120, 95);
  tft.printf("%.1f C", temperatura);

  // Umidade
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(15, 125);
  tft.print("Umid:");

  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(120, 125);
  tft.printf("%.1f %%", umidade);

  // Pressão
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(15, 155);
  tft.print("Press:");

  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(120, 155);
  tft.printf("%.1f hPa", pressao);

  // Altitude estimada
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(15, 185);
  tft.print("Alt:");

  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
  tft.setCursor(120, 185);
  tft.printf("%.1f m", altitude);
}

void atualizarMPU() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  // Limpa somente a área de conteúdo, preservando header/menu se existir
  tft.fillRect(0, 45, 280, 195, ST77XX_BLACK);

  tft.setTextWrap(false);

  // Título da tela
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
  tft.setCursor(15, 55);
  tft.print("Sensor MPU6050");

  // Linha separadora
  tft.drawFastHLine(15, 80, 250, ST77XX_MAGENTA);

  // Acelerômetro
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(15, 92);
  tft.print("ACELEROMETRO m/s2");

  tft.setTextSize(2);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(15, 110);
  tft.print("X:");
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(55, 110);
  tft.printf("%.2f", a.acceleration.x);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(145, 110);
  tft.print("Y:");
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(185, 110);
  tft.printf("%.2f", a.acceleration.y);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(15, 135);
  tft.print("Z:");
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(55, 135);
  tft.printf("%.2f", a.acceleration.z);

  // Giroscópio
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
  tft.setCursor(15, 165);
  tft.print("GIROSCOPIO rad/s");

  tft.setTextSize(2);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(15, 183);
  tft.print("X:");
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(55, 183);
  tft.printf("%.2f", g.gyro.x);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(145, 183);
  tft.print("Y:");
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(185, 183);
  tft.printf("%.2f", g.gyro.y);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(15, 208);
  tft.print("Z:");
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(55, 208);
  tft.printf("%.2f", g.gyro.z);

  // Temperatura interna do MPU
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(145, 208);
  tft.print("T:");
  tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);
  tft.setCursor(185, 208);
  tft.printf("%.1fC", t.temperature);
}

void ajustarBacklight() {
  if (estadoUI == TELA_UI && telaAtiva == TELA_AJUSTE_BRILHO) return;

  leituraIluminacao = analogRead(PIN_ILUMINACAO);

  if (leituraIluminacao > 1500) {
    ledcWrite(TFT_BLK, brilhoNoite);
  } else {
    ledcWrite(TFT_BLK, brilhoDia);
  }
}

void beep(int freq) {
  if (modoBuzzer == 0) return;

  ledcWriteTone(PIN_BUZZER, freq);
  delay(80);
  ledcWriteTone(PIN_BUZZER, 0);

  if (modoBuzzer == 2) {
    delay(80);
    ledcWriteTone(PIN_BUZZER, freq);
    delay(80);
    ledcWriteTone(PIN_BUZZER, 0);
  }
}
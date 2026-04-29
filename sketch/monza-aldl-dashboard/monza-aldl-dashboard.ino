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
#define PIN_SHIFT_LIGHT   35
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
#define TELA_AJUSTAR_HORA     2
#define TELA_ALDL             3
#define TELA_TESTE_I2C        4
#define TELA_TESTE_ELET       5
#define TELA_BME              6
#define TELA_MPU              7
#define TELA_STATUS_SD        8
#define TELA_TESTE_BUZZER     9
#define TELA_TESTE_DISPLAY    10
#define TELA_AJUSTE_BRILHO    11
#define TELA_SELECIONAR_GIF   12
#define TELA_CONFIG_BUZZER    13
#define TELA_OTA              14
#define TELA_TPS              15
#define TELA_MAP              16
#define TELA_CTS              17
#define TELA_IAT              18
#define TELA_VOLT             19
#define TELA_RPM              20
#define TELA_TEMPO_INJECAO    21
#define TELA_CO2              22
#define TELA_CODIGOS_ECU      23
#define TELA_LIMPAR_ECU       24
#define TELA_ALERTAS          25
#define TELA_DASH_DEFAULT     26
#define TELA_UPLOAD_GIFS      27
#define TELA_TEMPOS_UI        28

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
// UPLOAD GIFS VIA AP
// =======================
bool gifUploadAtivo = false;
bool gifUploadErro = false;
bool gifUploadEmAndamento = false;
String gifUploadStatusMsg = "Aguardando upload";
String gifUploadNomeArquivo = "";
int gifUploadPercent = 0;
File gifUploadFile;

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
const char* submenuDiagnostico[] = {"Status ALDL","Codigos ECU","Limpar erros ECU","Status MPU","Status BME","Status I2C","Status Iluminacao","SD Card","Teste Buzzer","Teste Display","Voltar"};
const char* submenuConfig[] = {"Data e hora","GIF abertura","Alertas","Brilho tela","Dash default","Buzzer","Tempos UI","Update via OTA","Upload Gifs","Voltar"};

Menu menuPrincipal = { "Menu Principal", menuPrincipalItens, 4, nullptr };
Menu menuSensores = { "Sensores", submenuSensores, 9, &menuPrincipal };
Menu menuDiagnostico = { "Diagnostico", submenuDiagnostico, 11, &menuPrincipal };
Menu menuConfig = { "Configuracao", submenuConfig, 10, &menuPrincipal };

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
unsigned long intervaloUpdateMs = 250;
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
int tempoMotorLigado = 0;
int errosChecksum = 0;
unsigned long ultimaMensagemALDL = 0;
int pacotesRecebidos = 0;
bool ecuConectada = false;
uint8_t malf1 = 0;
uint8_t malf2 = 0;
uint8_t malf3 = 0;
uint8_t lccpmw = 0;
float fanEstimadoLimite = 100.0;

// Variaveis Alertas
bool alertaTempMotorAtivo = true;
bool alertaTensaoAtivo = true;
bool alertaShiftLightAtivo = true;

float alertaTempMotorLimite = 105.0;
float alertaTensaoMinima = 11.5;
int alertaShiftLightRPM = 5500;

// Variaveis Dash
int dashAtual = 0;
int dashInicialDefault = -1; // -1 = nenhum
const int totalDashboards = 6;

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
unsigned long dashboardUiUpdateMs = 300;
unsigned long aldlUiUpdateMs = 180;

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
void desenharDashboardHeader(const char* titulo);
void desenharCardDash(int x, int y, int w, int h, const char* titulo, const char* valor, const char* unidade, uint16_t cor);
void dashboardRelogioBME();
void dashboardALDL1();
void dashboardALDL2();
void dashboardALDL3();
void dashboardALDL4();
void dashboardALDL5();
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
void telaUploadGifs();
void iniciarUploadGifs();
void pararUploadGifs();
void loopUploadGifs();
void configurarRotasUploadGifs();
String sanitizarNomeArquivoGif(String nome);
void telaSensorCO2();
void telaTemposUI();
void desenharLinhaTempoUI(int y, const char* nome, unsigned long valor, bool selecionado, bool editando);
void telaCodigosECU();
void telaLimparErrosECU();
void formatarTempoMotor(char* buffer, size_t tamanho, int segundos);
std::vector<String> obterFalhasAtivas();
void salvarConfiguracoes();
void carregarConfiguracoes();
void desenharAlertaTela(const char* titulo, const char* mensagem, uint16_t corFundo);
void * GIFOpenFile(const char *fname, int32_t *pSize);
void GIFCloseFile(void *pHandle);
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);
void GIFDraw(GIFDRAW *pDraw);
void rodarGifAbertura(String nomeArquivo);
void telaStatusBME();
void telaStatusMPU();
void telaAlerta();
void telaDashDefault();
bool verificarAlertas();
void ajustarBacklight();
void beep(int freq);
void desenharLinhaAlertaInt(int y, const char* nome, int valor, const char* unidade, bool selecionado, bool editando);
void desenharLinhaAlertaTexto(int y, const char* nome, const char* valor, bool selecionado, bool ativo);
void desenharLinhaAlertaValor(int y, const char* nome, float valor, const char* unidade, bool selecionado, bool editando);

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
void pararServidorWebAP();

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
  if (dashInicialDefault >= 0 && dashInicialDefault < totalDashboards) {
    dashAtual = dashInicialDefault;
    telaAtiva = TELA_DASHBOARD;
    estadoUI = TELA_UI;
    tft.fillScreen(ST77XX_BLACK);
    atualizarTela();
  } else {
    desenharMenu();
  }
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

  if (estadoUI == TELA_UI && telaAtiva == TELA_UPLOAD_GIFS) {
    loopUploadGifs();
  }

  ajustarBacklight();
  lerEncoder();

  bool alertaAtivo = verificarAlertas();

  if (!alertaAtivo && estadoUI == TELA_UI) {
    unsigned long intervaloAtual = intervaloUpdateMs;

    if (telaEhSensorALDL(telaAtiva)) intervaloAtual = aldlUiUpdateMs;
    if (telaAtiva == TELA_DASHBOARD) intervaloAtual = dashboardUiUpdateMs;
    if (telaAtiva == TELA_OTA || telaAtiva == TELA_UPLOAD_GIFS) intervaloAtual = 250;    

    if (millis() - ultimoUpdate >= intervaloAtual || movimentoEncoder != 0) {
      ultimoUpdate = millis();
      atualizarTela();
    }
  }

  delay(1);
}

bool verificarAlertas() {
  if (estadoUI != TELA_UI) return false;
  if (!telaEhSensorALDL(telaAtiva)) return false;

  // Nao dispara alerta antes do primeiro frame valido
  if (!aldlPrimeiroFrameOk) return false;
  if (!aldlTemFrameValido) return false;
  if (pacotesRecebidos <= 0) return false;

  // Garante que a ECU ainda esta viva
  ecuConectada = (millis() - ultimaMensagemALDL < ALDL_TIMEOUT_CONECTADA_MS);
  if (!ecuConectada) return false;

  if (alertaTempMotorAtivo && tempMotor >= alertaTempMotorLimite) {
    desenharAlertaTela("TEMP MOTOR", "ALTA", ST77XX_RED);
    return true;
  }

  if (alertaTensaoAtivo && voltagem > 0 && voltagem <= alertaTensaoMinima) {
    desenharAlertaTela("TENSAO", "BAIXA", ST77XX_ORANGE);
    return true;
  }

  if (alertaShiftLightAtivo && valorRPM >= alertaShiftLightRPM) {
    desenharAlertaTela("SHIFT", "TROCAR MARCHA", ST77XX_YELLOW);
    return true;
  }

  return false;
}

void desenharAlertaTela(const char* titulo, const char* mensagem, uint16_t corFundo) {
  static unsigned long ultimoDesenho = 0;
  static char ultimoTitulo[24] = "";
  static char ultimoMensagem[32] = "";
  static uint16_t ultimaCor = 0;
  static bool alertaDesenhado = false;


  bool mudouAlerta =
    strcmp(ultimoTitulo, titulo) != 0 ||
    strcmp(ultimoMensagem, mensagem) != 0 ||
    ultimaCor != corFundo;

  if (mudouAlerta) {
    strncpy(ultimoTitulo, titulo, sizeof(ultimoTitulo) - 1);
    ultimoTitulo[sizeof(ultimoTitulo) - 1] = '\0';

    strncpy(ultimoMensagem, mensagem, sizeof(ultimoMensagem) - 1);
    ultimoMensagem[sizeof(ultimoMensagem) - 1] = '\0';

    ultimaCor = corFundo;

    alertaDesenhado = false;
  }

  // Evita redesenhar sem necessidade
  if (alertaDesenhado && millis() - ultimoDesenho < 300) {
    return;
  }

  ultimoDesenho = millis();
  alertaDesenhado = true;

  tft.fillScreen(corFundo);
  tft.drawRoundRect(10, 45, 260, 150, 8, ST77XX_WHITE);

  tft.setTextWrap(false);

  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE, corFundo);

  int16_t x1, y1;
  uint16_t w, h;

  tft.getTextBounds(titulo, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((280 - w) / 2, 75);
  tft.print(titulo);

  tft.setTextSize(2);
  tft.getTextBounds(mensagem, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((280 - w) / 2, 125);
  tft.print(mensagem);

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
      telaAtiva != TELA_LIMPAR_ECU &&
      telaAtiva != TELA_MPU &&
      telaAtiva != TELA_BME &&
      telaAtiva != TELA_TESTE_I2C &&
      telaAtiva != TELA_TESTE_ELET &&
      telaAtiva != TELA_ALERTAS &&
      telaAtiva != TELA_TESTE_DISPLAY &&
      telaAtiva != TELA_UPLOAD_GIFS &&
      telaAtiva != TELA_TEMPOS_UI &&
      telaAtiva != TELA_DASH_DEFAULT) {

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
    else if (menuIndex == 3) { telaAtiva = TELA_MPU;           estadoUI = TELA_UI; }
    else if (menuIndex == 4) { telaAtiva = TELA_BME;           estadoUI = TELA_UI; }
    else if (menuIndex == 5) { telaAtiva = TELA_TESTE_I2C;     estadoUI = TELA_UI; }
    else if (menuIndex == 6) { telaAtiva = TELA_TESTE_ELET;    estadoUI = TELA_UI; }
    else if (menuIndex == 7) { telaAtiva = TELA_STATUS_SD;     estadoUI = TELA_UI; }
    else if (menuIndex == 8) { telaAtiva = TELA_TESTE_BUZZER;  estadoUI = TELA_UI; }
    else if (menuIndex == 9) { telaAtiva = TELA_TESTE_DISPLAY; estadoUI = TELA_UI; }
  }

  else if (menuAtual == &menuConfig) {
    if (menuIndex == 0) { telaAtiva = TELA_AJUSTAR_HORA; estadoUI = TELA_UI; }
    else if (menuIndex == 1) { telaAtiva = TELA_SELECIONAR_GIF; estadoUI = TELA_UI; }
    else if (menuIndex == 2) { telaAtiva = TELA_ALERTAS; estadoUI = TELA_UI; }
    else if (menuIndex == 3) { telaAtiva = TELA_AJUSTE_BRILHO; estadoUI = TELA_UI; }
    else if (menuIndex == 4) { telaAtiva = TELA_DASH_DEFAULT; estadoUI = TELA_UI; }
    else if (menuIndex == 5) { telaAtiva = TELA_CONFIG_BUZZER; estadoUI = TELA_UI; }
    else if (menuIndex == 6) { telaAtiva = TELA_TEMPOS_UI; estadoUI = TELA_UI; }
    else if (menuIndex == 7) { telaAtiva = TELA_OTA; estadoUI = TELA_UI; }
    else if (menuIndex == 8) { telaAtiva = TELA_UPLOAD_GIFS; estadoUI = TELA_UI; }
  }

  if (estadoUI == MENU_UI) {
    desenharMenu();
  } else {
    tft.fillScreen(ST77XX_BLACK);

    
    if (telaAtiva == TELA_OTA) {
      iniciarOTA();
    }

    if (telaAtiva == TELA_UPLOAD_GIFS) {
      iniciarUploadGifs();
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
    case TELA_BME:          telaStatusBME(); break;
    case TELA_MPU:          telaStatusMPU(); break;
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
    case TELA_ALERTAS: telaAlerta(); break;
    case TELA_DASH_DEFAULT: telaDashDefault(); break;
    case TELA_UPLOAD_GIFS: telaUploadGifs(); break;
    case TELA_TEMPOS_UI: telaTemposUI(); break;
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
  static bool iniciado = false;
  static int ultimoDash = -1;

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);
    iniciado = true;
    ultimoDash = -1;
  }

  if (movimentoEncoder != 0) {
    dashAtual += movimentoEncoder;

    if (dashAtual < 0) {
      dashAtual = totalDashboards - 1;
    }

    if (dashAtual >= totalDashboards) {
      dashAtual = 0;
    }

    movimentoEncoder = 0;
    ultimoDash = -1;
    beep(freqEncoder);
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoDash = -1;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
    return;
  }

  if (ultimoDash != dashAtual) {
    tft.fillScreen(ST77XX_BLACK);
    ultimoDash = dashAtual;
  }

  switch (dashAtual) {
    case 0:
      dashboardRelogioBME();
      break;

    case 1:
      dashboardALDL1();
      break;

    case 2:
      dashboardALDL2();
      break;

    case 3:
      dashboardALDL3();
      break;

    case 4:
      dashboardALDL4();
      break;

    case 5:
      dashboardALDL5();
      break;
  }
}

void desenharDashboardHeader(const char* titulo) {
  //tft.fillRect(0, 0, 280, 38, ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(titulo, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((280 - w) / 2, 10);
  tft.print(titulo);

  tft.drawFastHLine(25, 33, 230, ST77XX_GREY);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
  tft.setCursor(16, 240);
  tft.print("Gire: trocar dash | Clique: voltar");

  tft.setCursor(226, 240);
  tft.printf("%d/%d", dashAtual + 1, totalDashboards);
}

void desenharCardDash(int x, int y, int w, int h, const char* titulo, const char* valor, const char* unidade, uint16_t cor) {
  tft.fillRoundRect(x, y, w, h, 6, ST77XX_BLACK);
  tft.drawRoundRect(x, y, w, h, 6, cor);

  tft.setTextSize(1);
  tft.setTextColor(cor, ST77XX_BLACK);
  tft.setCursor(x + 8, y + 8);
  tft.print(titulo);

  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(x + 8, y + 27);
  tft.print(valor);

  if (strlen(unidade) > 0) {
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(x + 8, y + h - 14);
    tft.print(unidade);
  }
}

void dashboardRelogioBME() {
  DateTime now = rtc.now();
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;

  desenharDashboardHeader("DASH 1 - AMBIENTE");

  tft.setTextSize(5);
  tft.setTextColor(0x07FF, ST77XX_BLACK);
  tft.setCursor(50, 48);
  tft.printf("%02d:%02d", now.hour(), now.minute());

  tft.setTextSize(2);
  tft.setTextColor(0x03EF, ST77XX_BLACK);
  tft.setCursor(220, 65);
  tft.printf("%02d", now.second());

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(75, 95);
  tft.printf("%02d/%02d/%04d", now.day(), now.month(), now.year());

  char valorTemp[16];
  char valorHum[16];
  char valorPres[16];

  snprintf(valorTemp, sizeof(valorTemp), "%.1f", temp);
  snprintf(valorHum, sizeof(valorHum), "%.0f", hum);
  snprintf(valorPres, sizeof(valorPres), "%.0f", pres);

  desenharCardDash(18, 130, 75, 78, "TEMP", valorTemp, "C", ST77XX_CYAN);
  desenharCardDash(102, 130, 75, 78, "UMID", valorHum, "%", ST77XX_GREEN);
  desenharCardDash(186, 130, 75, 78, "PRESS", valorPres, "hPa", ST77XX_ORANGE);
}

void dashboardALDL1() {
  char vRPM[16];
  char vTPS[16];
  char vMAP[16];
  char vBAT[16];

  snprintf(vRPM, sizeof(vRPM), "%d", valorRPM);
  snprintf(vTPS, sizeof(vTPS), "%.1f", valorTPS);
  snprintf(vMAP, sizeof(vMAP), "%.2f", valorMAP);
  snprintf(vBAT, sizeof(vBAT), "%.1f", voltagem);

  desenharDashboardHeader("DASH 1 - MOTOR");

  desenharCardDash(15, 55, 120, 75, "RPM", vRPM, "rpm", ST77XX_YELLOW);
  desenharCardDash(145, 55, 120, 75, "TPS", vTPS, "%", ST77XX_CYAN);

  desenharCardDash(15, 145, 120, 75, "MAP", vMAP, "V", ST77XX_GREEN);
  desenharCardDash(145, 145, 120, 75, "BATERIA", vBAT, "V", ST77XX_ORANGE);
}

void dashboardALDL2() {
  char vCTS[16];
  char vIAT[16];
  char vINJ[16];
  char vCO2[16];

  snprintf(vCTS, sizeof(vCTS), "%.1f", tempMotor);
  snprintf(vIAT, sizeof(vIAT), "%.1f", tempAdmissao);
  snprintf(vINJ, sizeof(vINJ), "%.2f", tempoInjecao);
  snprintf(vCO2, sizeof(vCO2), "%.2f", voltCO2);

  desenharDashboardHeader("DASH 2 - SENSORES");

  desenharCardDash(15, 55, 120, 75, "TEMP MOTOR", vCTS, "C", ST77XX_RED);
  desenharCardDash(145, 55, 120, 75, "TEMP ADM", vIAT, "C", ST77XX_CYAN);

  desenharCardDash(15, 145, 120, 75, "INJECAO", vINJ, "ms", ST77XX_GREEN);
  desenharCardDash(145, 145, 120, 75, "CO2 POT", vCO2, "V", ST77XX_MAGENTA);
}

void dashboardALDL3() {
  char vVEL[16];
  char vECU[16];
  char vRX[16];
  char vCHK[16];

  ecuConectada = (millis() - ultimaMensagemALDL < ALDL_TIMEOUT_CONECTADA_MS);

  snprintf(vVEL, sizeof(vVEL), "%d", velocidade);
  snprintf(vECU, sizeof(vECU), "%s", ecuConectada ? "ON" : "OFF");
  snprintf(vRX, sizeof(vRX), "%d", pacotesRecebidos);
  snprintf(vCHK, sizeof(vCHK), "%d", errosChecksum);

  desenharDashboardHeader("DASH 3 - ECU");

  desenharCardDash(15, 55, 120, 75, "VELOC.", vVEL, "km/h", ST77XX_CYAN);
  desenharCardDash(145, 55, 120, 75, "ECU", vECU, ecuConectada ? "online" : "offline", ecuConectada ? ST77XX_GREEN : ST77XX_RED);

  desenharCardDash(15, 145, 120, 75, "PACOTES", vRX, "rx", ST77XX_GREEN);
  desenharCardDash(145, 145, 120, 75, "CHECKSUM", vCHK, "erros", ST77XX_ORANGE);
}

void dashboardALDL4() {
  char vRPM[16];
  char vTPS[16];
  char vMAP[16];
  char vVEL[16];

  snprintf(vRPM, sizeof(vRPM), "%d", valorRPM);
  snprintf(vTPS, sizeof(vTPS), "%.1f", valorTPS);
  snprintf(vMAP, sizeof(vMAP), "%.2f", valorMAP);
  snprintf(vVEL, sizeof(vVEL), "%d", velocidade);

  desenharDashboardHeader("DASH 4 - CUSTOM");

  desenharCardDash(15, 55, 120, 75, "RPM", vRPM, "rpm", ST77XX_YELLOW);
  desenharCardDash(145, 55, 120, 75, "TPS", vTPS, "%", ST77XX_CYAN);

  desenharCardDash(15, 145, 120, 75, "MAP", vMAP, "V", ST77XX_GREEN);
  desenharCardDash(145, 145, 120, 75, "VELOC.", vVEL, "km/h", ST77XX_ORANGE);
}

void dashboardALDL5() {
  char vFan[8];
  char vMotor[8];
  char vTempo[16];
  char vECU[8];

  bool fanEstimado = tempMotor >= fanEstimadoLimite;

  snprintf(vFan, sizeof(vFan), "%s", fanEstimado ? "ON" : "OFF");
  snprintf(vMotor, sizeof(vMotor), "%s", tempoMotorLigado > 0 ? "ON" : "OFF");
  snprintf(vECU, sizeof(vECU), "%s", ecuConectada ? "ON" : "OFF");

  formatarTempoMotor(vTempo, sizeof(vTempo), tempoMotorLigado);

  desenharDashboardHeader("DASH 5 - STATUS");

  desenharCardDash(15, 55, 120, 75, "FAN", vFan, "", fanEstimado ? ST77XX_GREEN : ST77XX_RED);
  desenharCardDash(145, 55, 120, 75, "MOTOR", vMotor, "", tempoMotorLigado > 0 ? ST77XX_GREEN : ST77XX_RED);

  desenharCardDash(15, 145, 120, 75, "TEMPO", vTempo, "motor ligado", ST77XX_CYAN);
  desenharCardDash(145, 145, 120, 75, "ECU", vECU, ecuConectada ? "online" : "offline", ecuConectada ? ST77XX_GREEN : ST77XX_RED);
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

  if (cliqueDetectado) {
    cliqueDetectado = false;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
    beep(freqClique);
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
    tela == TELA_DASHBOARD ||
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

void telaTemposUI() {
  static bool iniciado = false;
  static int opcao = 0;
  static int ultimaOpcao = -1;
  static bool editando = false;
  static bool precisaRedesenhar = true;

  const int totalOpcoes = 4;

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(65, 15);
    tft.print("TEMPOS UI");
    tft.drawFastHLine(30, 35, 220, ST77XX_GREY);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(15, 240);
    tft.print("Gire: navega/ajusta | Clique: seleciona");

    iniciado = true;
    opcao = 0;
    ultimaOpcao = -1;
    editando = false;
    precisaRedesenhar = true;
  }

  if (movimentoEncoder != 0) {
    if (editando) {
      unsigned long passo = 10;

      if (opcao == 0) {
        long novoValor = (long)intervaloUpdateMs + (movimentoEncoder * passo);
        if (novoValor < 50) novoValor = 50;
        if (novoValor > 2000) novoValor = 2000;
        intervaloUpdateMs = (unsigned long)novoValor;
      }

      else if (opcao == 1) {
        long novoValor = (long)dashboardUiUpdateMs + (movimentoEncoder * passo);
        if (novoValor < 50) novoValor = 50;
        if (novoValor > 2000) novoValor = 2000;
        dashboardUiUpdateMs = (unsigned long)novoValor;
      }

      else if (opcao == 2) {
        long novoValor = (long)aldlUiUpdateMs + (movimentoEncoder * passo);
        if (novoValor < 50) novoValor = 50;
        if (novoValor > 2000) novoValor = 2000;
        aldlUiUpdateMs = (unsigned long)novoValor;
      }
    } else {
      opcao += movimentoEncoder;

      if (opcao < 0) opcao = totalOpcoes - 1;
      if (opcao >= totalOpcoes) opcao = 0;
    }

    movimentoEncoder = 0;
    precisaRedesenhar = true;
    beep(freqEncoder);
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;

    switch (opcao) {
      case 0:
      case 1:
      case 2:
        editando = !editando;

        if (!editando) {
          salvarConfiguracoes();
          beep(freqSucesso);
        } else {
          beep(freqClique);
        }

        precisaRedesenhar = true;
        break;

      case 3:
        salvarConfiguracoes();

        iniciado = false;
        opcao = 0;
        ultimaOpcao = -1;
        editando = false;
        precisaRedesenhar = true;

        estadoUI = MENU_UI;
        tft.fillScreen(ST77XX_BLACK);
        desenharMenu();
        beep(freqClique);
        return;
    }
  }

  if (precisaRedesenhar || ultimaOpcao != opcao) {
    tft.fillRect(0, 55, 280, 175, ST77XX_BLACK);

    desenharLinhaTempoUI(65, "Padrao", intervaloUpdateMs, opcao == 0, editando && opcao == 0);
    desenharLinhaTempoUI(105, "Dashboard", dashboardUiUpdateMs, opcao == 1, editando && opcao == 1);
    desenharLinhaTempoUI(145, "ALDL UI", aldlUiUpdateMs, opcao == 2, editando && opcao == 2);

    bool selecionadoVoltar = opcao == 3;
    uint16_t corFundo = selecionadoVoltar ? ST77XX_BLUE : ST77XX_BLACK;

    if (selecionadoVoltar) {
      tft.fillRoundRect(8, 185, 264, 28, 5, corFundo);
    }

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, corFundo);
    tft.setCursor(18, 190);
    tft.print("Voltar");

    if (editando) {
      tft.fillRect(0, 222, 280, 12, ST77XX_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);
      tft.setCursor(20, 222);
      tft.print("EDITANDO: passo 10ms, clique salva");
    } else {
      tft.fillRect(0, 222, 280, 12, ST77XX_BLACK);
    }

    ultimaOpcao = opcao;
    precisaRedesenhar = false;
  }
}

void desenharLinhaTempoUI(int y, const char* nome, unsigned long valor, bool selecionado, bool editando) {
  uint16_t corFundo = selecionado ? ST77XX_BLUE : ST77XX_BLACK;
  uint16_t corValor = editando ? ST77XX_ORANGE : ST77XX_CYAN;

  if (selecionado) {
    tft.fillRoundRect(8, y - 6, 264, 30, 5, corFundo);
  }

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, corFundo);
  tft.setCursor(18, y);
  tft.print(nome);

  tft.setTextColor(corValor, corFundo);
  tft.setCursor(170, y);
  tft.printf("%lu", valor);

  tft.setTextSize(1);
  tft.setTextColor(corValor, corFundo);
  tft.setCursor(235, y + 7);
  tft.print("ms");
}

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

  doc["dashInicialDefault"] = dashInicialDefault;

  doc["alertaTempMotorAtivo"] = alertaTempMotorAtivo;
  doc["alertaTensaoAtivo"] = alertaTensaoAtivo;
  doc["alertaShiftLightAtivo"] = alertaShiftLightAtivo;
  doc["alertaTempMotorLimite"] = alertaTempMotorLimite;
  doc["alertaTensaoMinima"] = alertaTensaoMinima;
  doc["alertaShiftLightRPM"] = alertaShiftLightRPM;

  doc["intervaloUpdateMs"] = intervaloUpdateMs;
  doc["dashboardUiUpdateMs"] = dashboardUiUpdateMs;
  doc["aldlUiUpdateMs"] = aldlUiUpdateMs;

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
    alertaTempMotorAtivo = doc["alertaTempMotorAtivo"] | true;
    alertaTensaoAtivo = doc["alertaTensaoAtivo"] | true;
    alertaShiftLightAtivo = doc["alertaShiftLightAtivo"] | true;
    alertaTempMotorLimite = doc["alertaTempMotorLimite"] | 105.0;
    alertaTensaoMinima = doc["alertaTensaoMinima"] | 11.5;
    alertaShiftLightRPM = doc["alertaShiftLightRPM"] | 5500;

    dashInicialDefault = doc["dashInicialDefault"] | -1;

    intervaloUpdateMs = doc["intervaloUpdateMs"] | 250;
    dashboardUiUpdateMs = doc["dashboardUiUpdateMs"] | 300;
    aldlUiUpdateMs = doc["aldlUiUpdateMs"] | 180;
  }
  if (dashInicialDefault < -1) dashInicialDefault = -1;
  if (dashInicialDefault >= totalDashboards) dashInicialDefault = -1;

  if (intervaloUpdateMs < 50) intervaloUpdateMs = 50;
  if (intervaloUpdateMs > 2000) intervaloUpdateMs = 2000;

  if (dashboardUiUpdateMs < 50) dashboardUiUpdateMs = 50;
  if (dashboardUiUpdateMs > 2000) dashboardUiUpdateMs = 2000;

  if (aldlUiUpdateMs < 50) aldlUiUpdateMs = 50;
  if (aldlUiUpdateMs > 2000) aldlUiUpdateMs = 2000;

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
  tempoMotorLigado = 0;
  lccpmw = 0;
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
  uint8_t mapRaw  = lerPayloadByte(frame, 26);
  uint8_t matRaw = lerPayloadByte(frame, 30);
  uint8_t battRaw  = lerPayloadByte(frame, 32);
  uint8_t bpwMsb   = lerPayloadByte(frame, 36);
  uint8_t bpwLsb   = lerPayloadByte(frame, 37);
  uint8_t timeEng   = lerPayloadByte(frame, 41);
  uint8_t lccpmwRaw = lerPayloadByte(frame, 58);

  malf1 = malf1Raw;
  malf2 = malf2Raw;
  malf3 = malf3Raw;
  lccpmw = lccpmwRaw;

  tempMotor = (tempRaw * 0.75f) - 40.0f;
  tempAdmissao = (matRaw * 0.75f) - 40.0f;
  valorTPS = ptpsRaw / 2.55f;
  valorRPM = (int)(rpmRaw * 25.0f);
  velocidade = velRaw;
  voltagem = battRaw / 10.0f;
  voltCO2 = CO2Raw * (5.0f / 255.0f);
  valorMAP = mapRaw * (5.0f / 255.0f);
  tempoInjecao = ((bpwMsb * 256.0f) + bpwLsb) / 65.536f;
  tempoMotorLigado = timeEng;
  //shiftLightALDL = (lccpmw & (1 << 7)) == 0;

  ultimaMensagemALDL = millis();
  pacotesRecebidos++;
  ecuConectada = true;
}

void formatarTempoMotor(char* buffer, size_t tamanho, int segundos) {
  int horas = segundos / 3600;
  int minutos = (segundos % 3600) / 60;
  int seg = segundos % 60;

  if (horas > 0) {
    snprintf(buffer, tamanho, "%02d:%02d:%02d", horas, minutos, seg);
  } else {
    snprintf(buffer, tamanho, "%02d:%02d", minutos, seg);
  }
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
  pararServidorWebAP();

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

void pararServidorWebAP() {
  if (gifUploadFile) {
    gifUploadFile.close();
  }

  otaServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  otaAtivo = false;
  gifUploadAtivo = false;
  gifUploadErro = false;

  otaUploadEmAndamento = false;
  otaReiniciarPendente = false;
  otaStatusMsg = "Aguardando upload";
  otaPercent = 0;

  gifUploadEmAndamento = false;
  gifUploadStatusMsg = "Aguardando upload";
  gifUploadNomeArquivo = "";
  gifUploadPercent = 0;

  otaIP = "";
}

void pararOTA() {
  pararServidorWebAP();
}

void loopOTA() {
  if (!otaAtivo) return;

  otaServer.handleClient();

  if (otaReiniciarPendente && millis() >= otaReiniciarEm) {
    ESP.restart();
  }
}

// ======================================================
// FUNCOES GIF
// ======================================================
String sanitizarNomeArquivoGif(String nome) {
  nome.replace("\\", "/");

  int barra = nome.lastIndexOf('/');
  if (barra >= 0) {
    nome = nome.substring(barra + 1);
  }

  nome.trim();
  nome.toLowerCase();

  nome.replace(" ", "_");
  nome.replace("á", "a");
  nome.replace("à", "a");
  nome.replace("ã", "a");
  nome.replace("â", "a");
  nome.replace("é", "e");
  nome.replace("ê", "e");
  nome.replace("í", "i");
  nome.replace("ó", "o");
  nome.replace("ô", "o");
  nome.replace("õ", "o");
  nome.replace("ú", "u");
  nome.replace("ç", "c");

  String limpo = "";

  for (int i = 0; i < nome.length(); i++) {
    char c = nome[i];

    bool permitido =
      (c >= 'a' && c <= 'z') ||
      (c >= '0' && c <= '9') ||
      c == '_' ||
      c == '-' ||
      c == '.';

    if (permitido) {
      limpo += c;
    }
  }

  if (!limpo.endsWith(".gif")) {
    limpo += ".gif";
  }

  if (limpo.length() < 5) {
    limpo = "upload.gif";
  }

  return "/" + limpo;
}

void configurarRotasUploadGifs() {
  otaServer.on("/", HTTP_GET, []() {
    String html;
    html += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Monza Dash - Upload GIFs</title>";
    html += "</head><body style='font-family:Arial;background:#111;color:#eee;padding:20px'>";
    html += "<h2>Monza Dash - Upload GIFs</h2>";
    html += "<p>Use GIF .gif em 280x240. Evite arquivos muito grandes.</p>";
    html += "<p>Depois voce pode selecionar o GIF em Configuracao &gt; GIF abertura.</p>";
    html += "<form method='POST' action='/uploadgif' enctype='multipart/form-data'>";
    html += "<input type='file' name='gif' accept='.gif,image/gif'><br><br>";
    html += "<input type='submit' value='Enviar GIF'>";
    html += "</form>";
    html += "<hr>";
    html += "<p>Status atual: " + gifUploadStatusMsg + "</p>";
    html += "<p>Arquivo: " + gifUploadNomeArquivo + "</p>";
    html += "<p><a style='color:#6cf' href='/listar'>Listar GIFs no SD</a></p>";
    html += "</body></html>";

    otaServer.send(200, "text/html", html);
  });

  otaServer.on("/listar", HTTP_GET, []() {
    String html;
    html += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>GIFs no SD</title>";
    html += "</head><body style='font-family:Arial;background:#111;color:#eee;padding:20px'>";
    html += "<h2>GIFs no SD</h2>";
    html += "<ul>";

    File root = SD.open("/");
    if (root) {
      File file = root.openNextFile();

      while (file) {
        String nome = String(file.name());
        String nomeLower = nome;
        nomeLower.toLowerCase();

        if (!file.isDirectory() && nomeLower.endsWith(".gif")) {
          html += "<li>";
          html += nome;
          html += " - ";
          html += String(file.size());
          html += " bytes";
          html += "</li>";
        }

        file.close();
        file = root.openNextFile();
      }

      root.close();
    }

    html += "</ul>";
    html += "<p><a style='color:#6cf' href='/'>Voltar</a></p>";
    html += "</body></html>";

    otaServer.send(200, "text/html", html);
  });

  otaServer.on("/uploadgif", HTTP_POST,
    []() {
      bool ok = !gifUploadErro && !gifUploadEmAndamento && gifUploadNomeArquivo.length() > 0;

      otaServer.sendHeader("Connection", "close");

      String resposta = ok
        ? "Upload concluido: " + gifUploadNomeArquivo
        : "Falha no upload do GIF";

      otaServer.send(200, "text/plain", resposta);

      if (ok) {
        gifUploadStatusMsg = "Upload concluido";
        gifUploadPercent = 100;
      } else {
        gifUploadStatusMsg = "Erro no upload";
      }
    },
    []() {
      HTTPUpload& upload = otaServer.upload();

      if (upload.status == UPLOAD_FILE_START) {
        gifUploadEmAndamento = true;
        gifUploadPercent = 0;
        gifUploadStatusMsg = "Iniciando";
        gifUploadNomeArquivo = sanitizarNomeArquivoGif(upload.filename);

        if (!SD.begin(SD_CS)) {
          gifUploadStatusMsg = "SD nao iniciado";
          gifUploadEmAndamento = false;
          gifUploadErro = true;
          gifUploadNomeArquivo = "";
          return;
        }

        if (SD.exists(gifUploadNomeArquivo)) {
          SD.remove(gifUploadNomeArquivo);
        }

        gifUploadFile = SD.open(gifUploadNomeArquivo, FILE_WRITE);

        if (!gifUploadFile) {
          gifUploadStatusMsg = "Erro criando arquivo";
          gifUploadEmAndamento = false;
          gifUploadErro = true;
          gifUploadNomeArquivo = "";
          return;
        }
      }

      else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!gifUploadFile) {
          gifUploadStatusMsg = "Arquivo invalido";
          gifUploadErro = true;
          return;
        }

        size_t escrito = gifUploadFile.write(upload.buf, upload.currentSize);

        if (escrito != upload.currentSize) {
          gifUploadStatusMsg = "Erro gravando SD";
          gifUploadErro = true;
        } else {
          gifUploadStatusMsg = "Gravando GIF";
          gifUploadPercent = 50;
        }
      }
      else if (upload.status == UPLOAD_FILE_END) {
        if (gifUploadFile) {
          gifUploadFile.close();
        }

        gifUploadEmAndamento = false;

        if (!gifUploadErro) {
          gifUploadPercent = 100;
          gifUploadStatusMsg = "Upload finalizado";
        } else {
          gifUploadPercent = 0;
          gifUploadStatusMsg = "Erro no upload";
        }
      }

      else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (gifUploadFile) {
          gifUploadFile.close();
        }

        if (gifUploadNomeArquivo.length() > 0 && SD.exists(gifUploadNomeArquivo)) {
          SD.remove(gifUploadNomeArquivo);
        }

        gifUploadPercent = 0;
        gifUploadStatusMsg = "Upload cancelado";
        gifUploadEmAndamento = false;
        gifUploadErro = true;
      }
    }
  );
}

void iniciarUploadGifs() {
  pararServidorWebAP();

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(otaSSID.c_str(), otaSenha.c_str());

  if (ok) {
    IPAddress ip = WiFi.softAPIP();
    otaIP = ip.toString();

    gifUploadStatusMsg = "AP iniciado";
    gifUploadNomeArquivo = "";
    gifUploadPercent = 0;
    gifUploadEmAndamento = false;
    gifUploadErro = false;

    configurarRotasUploadGifs();
    otaServer.begin();
    gifUploadAtivo = true;
  } else {
    otaIP = "0.0.0.0";
    gifUploadStatusMsg = "Falha ao iniciar AP";
    gifUploadAtivo = false;
  }
}

void pararUploadGifs() {
  pararServidorWebAP();
}

void loopUploadGifs() {
  if (gifUploadAtivo) {
    otaServer.handleClient();
  }
}

// ======================================================
// OUTRAS FUNCOES
// ======================================================
void telaUploadGifs() {
  static bool iniciado = false;
  static String ultimoStatus = "";
  static int ultimoPercent = -1;
  static String ultimoIP = "";

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(55, 15);
    tft.print("UPLOAD GIFS");
    tft.drawFastHLine(30, 35, 220, ST77XX_GREY);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(30, 240);
    tft.print("Acesse pelo navegador | Clique sai");

    iniciado = true;
    ultimoStatus = "";
    ultimoPercent = -1;
    ultimoIP = "";
  }

  if (ultimoIP != otaIP) {
    tft.fillRect(0, 55, 280, 30, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(25, 60);
    tft.print("IP:");
    tft.setCursor(70, 60);
    tft.print(otaIP);

    ultimoIP = otaIP;
  }

  if (ultimoStatus != gifUploadStatusMsg || ultimoPercent != gifUploadPercent) {
    tft.fillRect(0, 100, 280, 100, ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(20, 105);
    tft.print("Status:");

    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(20, 130);
    tft.print(gifUploadStatusMsg);

    if (gifUploadNomeArquivo.length() > 0) {
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.setCursor(20, 158);
      tft.print("Arquivo: ");
      tft.print(gifUploadNomeArquivo);
    }

    tft.drawRect(20, 180, 240, 16, ST77XX_CYAN);

    int largura = map(gifUploadPercent, 0, 100, 0, 236);
    if (largura < 0) largura = 0;
    if (largura > 236) largura = 236;

    tft.fillRect(22, 182, largura, 12, ST77XX_GREEN);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(120, 202);
    tft.printf("%d%%", gifUploadPercent);

    ultimoStatus = gifUploadStatusMsg;
    ultimoPercent = gifUploadPercent;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;

    iniciado = false;
    ultimoStatus = "";
    ultimoPercent = -1;
    ultimoIP = "";

    pararUploadGifs();

    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
    beep(freqClique);
  }
}

void telaStatusBME() {
  static bool iniciado = false;
  static float ultimaTemperatura = -9999;
  static float ultimaUmidade = -9999;
  static float ultimaPressao = -9999;
  static float ultimaAltitude = -9999;

  float temperatura = bme.readTemperature();
  float umidade = bme.readHumidity();
  float pressao = bme.readPressure() / 100.0F;
  float altitude = bme.readAltitude(1013.25);

  if (!iniciado) {
    desenharTituloTelaSensor("BME280");
    desenharRodapeSensor();

    iniciado = true;
    ultimaTemperatura = -9999;
    ultimaUmidade = -9999;
    ultimaPressao = -9999;
    ultimaAltitude = -9999;
  }

  if (
    temperatura != ultimaTemperatura ||
    umidade != ultimaUmidade ||
    pressao != ultimaPressao ||
    altitude != ultimaAltitude
  ) {
    tft.fillRect(0, 50, 280, 175, ST77XX_BLACK);

    tft.setTextWrap(false);

    // Temperatura
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 60);
    tft.print("TEMP:");

    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(130, 60);
    tft.printf("%.1f C", temperatura);

    // Umidade
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 95);
    tft.print("UMID:");

    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(130, 95);
    tft.printf("%.1f %%", umidade);

    // Pressao
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 130);
    tft.print("PRESS:");

    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(130, 130);
    tft.printf("%.1f", pressao);

    tft.setTextSize(1);
    tft.setCursor(220, 136);
    tft.print("hPa");

    // Altitude
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 165);
    tft.print("ALT:");

    tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
    tft.setCursor(130, 165);
    tft.printf("%.1f m", altitude);

    ultimaTemperatura = temperatura;
    ultimaUmidade = umidade;
    ultimaPressao = pressao;
    ultimaAltitude = altitude;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimaTemperatura = -9999;
    ultimaUmidade = -9999;
    ultimaPressao = -9999;
    ultimaAltitude = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
    beep(freqClique);
  }
}

void telaStatusMPU() {
  static bool iniciado = false;
  static float ultimoAx = -9999;
  static float ultimoAy = -9999;
  static float ultimoAz = -9999;
  static float ultimoGx = -9999;
  static float ultimoGy = -9999;
  static float ultimoGz = -9999;
  static float ultimaTemp = -9999;

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  if (!iniciado) {
    desenharTituloTelaSensor("MPU6050");
    desenharRodapeSensor();

    iniciado = true;
    ultimoAx = -9999;
    ultimoAy = -9999;
    ultimoAz = -9999;
    ultimoGx = -9999;
    ultimoGy = -9999;
    ultimoGz = -9999;
    ultimaTemp = -9999;
  }

  if (
    a.acceleration.x != ultimoAx ||
    a.acceleration.y != ultimoAy ||
    a.acceleration.z != ultimoAz ||
    g.gyro.x != ultimoGx ||
    g.gyro.y != ultimoGy ||
    g.gyro.z != ultimoGz ||
    t.temperature != ultimaTemp
  ) {
    tft.fillRect(0, 50, 280, 175, ST77XX_BLACK);

    tft.setTextWrap(false);

    // Acelerometro
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(20, 55);
    tft.print("ACELEROMETRO m/s2");

    tft.setTextSize(2);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 75);
    tft.print("X:");
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(60, 75);
    tft.printf("%.2f", a.acceleration.x);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(145, 75);
    tft.print("Y:");
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(180, 75);
    tft.printf("%.2f", a.acceleration.y);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 103);
    tft.print("Z:");
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(60, 103);
    tft.printf("%.2f", a.acceleration.z);

    // Giroscopio
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
    tft.setCursor(20, 133);
    tft.print("GIROSCOPIO rad/s");

    tft.setTextSize(2);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 153);
    tft.print("X:");
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(60, 153);
    tft.printf("%.2f", g.gyro.x);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(145, 153);
    tft.print("Y:");
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(180, 153);
    tft.printf("%.2f", g.gyro.y);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(25, 181);
    tft.print("Z:");
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(60, 181);
    tft.printf("%.2f", g.gyro.z);

    // Temperatura interna
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(145, 181);
    tft.print("T:");
    tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);
    tft.setCursor(180, 181);
    tft.printf("%.1fC", t.temperature);

    ultimoAx = a.acceleration.x;
    ultimoAy = a.acceleration.y;
    ultimoAz = a.acceleration.z;
    ultimoGx = g.gyro.x;
    ultimoGy = g.gyro.y;
    ultimoGz = g.gyro.z;
    ultimaTemp = t.temperature;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;
    iniciado = false;
    ultimoAx = -9999;
    ultimoAy = -9999;
    ultimoAz = -9999;
    ultimoGx = -9999;
    ultimoGy = -9999;
    ultimoGz = -9999;
    ultimaTemp = -9999;
    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
    beep(freqClique);
  }
}

void telaDashDefault() {
  static bool iniciado = false;
  static int opcao = 0;
  static int ultimaOpcao = -1;

  const int totalOpcoes = totalDashboards + 1;

  const char* nomesDash[totalDashboards] = {
    "Ambiente",
    "Motor",
    "Sensores",
    "ECU",
    "Custom",
    "Status"
  };

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(45, 15);
    tft.print("DASH INICIAL");
    tft.drawFastHLine(30, 35, 220, ST77XX_GREY);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(20, 240);
    tft.print("Gire: escolher | Clique: salvar");

    // -1 vira opcao 0 = Nenhum
    //  0 vira opcao 1 = Dash 1
    //  1 vira opcao 2 = Dash 2...
    opcao = dashInicialDefault + 1;

    if (opcao < 0) opcao = 0;
    if (opcao >= totalOpcoes) opcao = 0;

    ultimaOpcao = -1;
    iniciado = true;
  }

  if (movimentoEncoder != 0) {
    opcao += movimentoEncoder;

    if (opcao < 0) {
      opcao = totalOpcoes - 1;
    }

    if (opcao >= totalOpcoes) {
      opcao = 0;
    }

    movimentoEncoder = 0;
    ultimaOpcao = -1;
    beep(freqEncoder);
  }

  if (ultimaOpcao != opcao) {
    tft.fillRect(0, 50, 280, 180, ST77XX_BLACK);

    for (int i = 0; i < totalOpcoes; i++) {
      int y = 55 + (i * 25);

      bool selecionado = i == opcao;

      int valorDash = i - 1; // opcao 0 = -1 = Nenhum
      bool atualDefault = valorDash == dashInicialDefault;

      uint16_t corFundo = selecionado ? ST77XX_BLUE : ST77XX_BLACK;

      if (selecionado) {
        tft.fillRoundRect(8, y - 4, 264, 23, 5, corFundo);
      }

      tft.setTextSize(2);
      tft.setTextColor(ST77XX_WHITE, corFundo);
      tft.setCursor(18, y);

      if (i == 0) {
        tft.print("Nenhum");
      } else {
        tft.printf("Dash %d", i);
      }

      tft.setTextSize(1);

      if (i == 0) {
        tft.setTextColor(selecionado ? ST77XX_YELLOW : ST77XX_GREY, corFundo);
        tft.setCursor(115, y + 6);
        tft.print("abrir menu");
      } else {
        tft.setTextColor(selecionado ? ST77XX_YELLOW : ST77XX_CYAN, corFundo);
        tft.setCursor(105, y + 6);
        tft.print(nomesDash[i - 1]);
      }

      if (atualDefault) {
        tft.setTextColor(ST77XX_GREEN, corFundo);
        tft.setCursor(220, y + 6);
        tft.print("ATUAL");
      }
    }

    ultimaOpcao = opcao;
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;

    dashInicialDefault = opcao - 1;

    if (dashInicialDefault < -1) {
      dashInicialDefault = -1;
    }

    if (dashInicialDefault >= totalDashboards) {
      dashInicialDefault = -1;
    }

    salvarConfiguracoes();

    iniciado = false;
    ultimaOpcao = -1;

    estadoUI = MENU_UI;
    tft.fillScreen(ST77XX_BLACK);
    desenharMenu();
    beep(freqSucesso);
  }
}

void telaAlerta() {
  static bool iniciado = false;
  static int opcao = 0;
  static int ultimaOpcao = -1;
  static bool editandoValor = false;
  static bool precisaRedesenhar = true;
  
  const int totalOpcoes = 7;

  if (!iniciado) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(75, 15);
    tft.print("ALERTAS");
    tft.drawFastHLine(30, 35, 220, ST77XX_GREY);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(15, 245);
    tft.print("Gire: navega/ajusta | Clique: seleciona");

    iniciado = true;
    opcao = 0;
    ultimaOpcao = -1;
    editandoValor = false;
    precisaRedesenhar = true;
  }

  if (movimentoEncoder != 0) {
    if (editandoValor) {
      switch (opcao) {
        case 1:
          alertaTempMotorLimite += movimentoEncoder;
          if (alertaTempMotorLimite < 70.0) alertaTempMotorLimite = 70.0;
          if (alertaTempMotorLimite > 130.0) alertaTempMotorLimite = 130.0;
          break;

        case 3:
          alertaTensaoMinima += movimentoEncoder * 0.1;
          if (alertaTensaoMinima < 9.0) alertaTensaoMinima = 9.0;
          if (alertaTensaoMinima > 13.0) alertaTensaoMinima = 13.0;
          alertaTensaoMinima = round(alertaTensaoMinima * 10.0) / 10.0;
          break;

        case 5:
          alertaShiftLightRPM += movimentoEncoder * 100;
          if (alertaShiftLightRPM < 1500) alertaShiftLightRPM = 1500;
          if (alertaShiftLightRPM > 7000) alertaShiftLightRPM = 7000;
          break;
      }
    } else {
      opcao += movimentoEncoder;

      if (opcao < 0) opcao = totalOpcoes - 1;
      if (opcao >= totalOpcoes) opcao = 0;
    }

    movimentoEncoder = 0;
    precisaRedesenhar = true;
    beep(freqEncoder);
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;

    switch (opcao) {
      case 0:
        alertaTempMotorAtivo = !alertaTempMotorAtivo;
        salvarConfiguracoes();
        precisaRedesenhar = true;
        beep(freqSucesso);
        break;

      case 1:
        editandoValor = !editandoValor;

        if (!editandoValor) {
          salvarConfiguracoes();
          beep(freqSucesso);
        } else {
          beep(freqClique);
        }

        precisaRedesenhar = true;
        break;

      case 2:
        alertaTensaoAtivo = !alertaTensaoAtivo;
        salvarConfiguracoes();
        precisaRedesenhar = true;
        beep(freqSucesso);
        break;

      case 3:
        editandoValor = !editandoValor;

        if (!editandoValor) {
          salvarConfiguracoes();
          beep(freqSucesso);
        } else {
          beep(freqClique);
        }

        precisaRedesenhar = true;
        break;

      case 4:
        alertaShiftLightAtivo = !alertaShiftLightAtivo;
        salvarConfiguracoes();
        precisaRedesenhar = true;
        beep(freqSucesso);
        break;

      case 5:
        editandoValor = !editandoValor;

        if (!editandoValor) {
          salvarConfiguracoes();
          beep(freqSucesso);
        } else {
          beep(freqClique);
        }

        precisaRedesenhar = true;
        break;

      case 6:
        iniciado = false;
        opcao = 0;
        ultimaOpcao = -1;
        precisaRedesenhar = true;
        editandoValor = false;
        estadoUI = MENU_UI;
        tft.fillScreen(ST77XX_BLACK);
        desenharMenu();
        beep(freqClique);
        return;
    }
  }

  if (precisaRedesenhar || ultimaOpcao != opcao) {
    tft.fillRect(0, 50, 280, 185, ST77XX_BLACK);

    desenharLinhaAlertaTexto(55, "Temp motor", alertaTempMotorAtivo ? "ON" : "OFF", opcao == 0, alertaTempMotorAtivo);
    desenharLinhaAlertaValor(82, "Limite temp", alertaTempMotorLimite, "C", opcao == 1, editandoValor && opcao == 1);

    desenharLinhaAlertaTexto(109, "Tensao bat", alertaTensaoAtivo ? "ON" : "OFF", opcao == 2, alertaTensaoAtivo);
    desenharLinhaAlertaValor(136, "Min tensao", alertaTensaoMinima, "V", opcao == 3, editandoValor && opcao == 3);

    desenharLinhaAlertaTexto(163, "Shift Light", alertaShiftLightAtivo ? "ON" : "OFF", opcao == 4, alertaShiftLightAtivo);
    desenharLinhaAlertaInt(190, "Shift RPM", alertaShiftLightRPM, "rpm", opcao == 5, editandoValor && opcao == 5);

    desenharLinhaAlertaTexto(217, "Voltar", "", opcao == 6, true);

    ultimaOpcao = opcao;
    precisaRedesenhar = false;
  }
}

void desenharLinhaAlertaInt(int y, const char* nome, int valor, const char* unidade, bool selecionado, bool editando) {
  uint16_t corFundo = selecionado ? ST77XX_BLUE : ST77XX_BLACK;
  uint16_t corValor = editando ? ST77XX_ORANGE : ST77XX_CYAN;

  if (selecionado) {
    tft.fillRoundRect(8, y - 5, 264, 25, 5, corFundo);
  }

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, corFundo);
  tft.setCursor(18, y);
  tft.print(nome);

  tft.setTextColor(corValor, corFundo);
  tft.setCursor(180, y);
  tft.printf("%d", valor);

  tft.setTextSize(1);
  tft.setTextColor(corValor, corFundo);
  tft.setCursor(240, y + 7);
  tft.print(unidade);

  if (editando) {
    tft.fillRect(0, 232, 280, 10, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);
    tft.setCursor(20, 232);
    tft.print("EDITANDO: gire ajusta 100rpm, clique salva");
  }
}

void desenharLinhaAlertaTexto(int y, const char* nome, const char* valor, bool selecionado, bool ativo) {
  uint16_t corFundo = selecionado ? ST77XX_BLUE : ST77XX_BLACK;

  if (selecionado) {
    tft.fillRoundRect(8, y - 5, 264, 25, 5, corFundo);
  }

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, corFundo);
  tft.setCursor(18, y);
  tft.print(nome);

  if (strlen(valor) > 0) {
    tft.setTextColor(ativo ? ST77XX_GREEN : ST77XX_RED, corFundo);
    tft.setCursor(220, y);
    tft.print(valor);
  }
}

void desenharLinhaAlertaValor(int y, const char* nome, float valor, const char* unidade, bool selecionado, bool editando) {
  uint16_t corFundo = selecionado ? ST77XX_BLUE : ST77XX_BLACK;
  uint16_t corValor = editando ? ST77XX_ORANGE : ST77XX_CYAN;

  if (selecionado) {
    tft.fillRoundRect(8, y - 5, 264, 25, 5, corFundo);
  }

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, corFundo);
  tft.setCursor(18, y);
  tft.print(nome);

  tft.setTextColor(corValor, corFundo);
  tft.setCursor(190, y);

  if (strcmp(unidade, "V") == 0) {
    tft.printf("%.1f%s", valor, unidade);
  } else {
    tft.printf("%.0f%s", valor, unidade);
  }

  if (editando) {
    tft.fillRect(0, 230, 280, 12, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);
    tft.setCursor(20, 230);
    tft.print("EDITANDO: gire para ajustar, clique salva");
  }
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
// ======================================================
// APP CELULAR - TELEMETRIA VIA WI-FI
// ======================================================
//
// O ESP32 fica em modo AP (mesmo SSID/senha do OTA) e o app Android
// conecta via TCP em 192.168.4.1:APP_TCP_PORTA.
//
// A cada APP_INTERVALO_ENVIO_MS e enviada uma linha JSON terminada em '\n'
// com os dados ALDL para cada celular conectado.
//
// Comandos aceitos (texto, terminado em '\n'):
//   CLR  -> envia o comando de limpar erros para a ECU
//
// Variaveis globais ficam no arquivo principal.

// ======================================================
// ACCESS POINT COMPARTILHADO (APP + OTA + UPLOAD GIFS)
// ======================================================

bool garantirAP() {
  if (apLigado) return true;

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(otaSSID.c_str(), otaSenha.c_str());
  apLigado = ok;

  if (ok) {
    otaIP = WiFi.softAPIP().toString();
  }

  return ok;
}

void desligarAPSeOcioso() {
  if (!apLigado) return;
  if (appWifiServidorAtivo || otaAtivo || gifUploadAtivo) return;

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  apLigado = false;
}

void iniciarWifiApp() {
  if (appWifiServidorAtivo) return;
  if (!garantirAP()) return;

  appTcpServer.begin();
  appTcpServer.setNoDelay(true);
  appWifiServidorAtivo = true;
}

void pararWifiApp() {
  for (int i = 0; i < APP_MAX_CLIENTES_TCP; i++) {
    if (appTcpClientes[i]) appTcpClientes[i].stop();
    appTcpBufferLen[i] = 0;
  }

  if (appWifiServidorAtivo) {
    appTcpServer.end();
  }

  appWifiServidorAtivo = false;
  desligarAPSeOcioso();
}

void aplicarConfigAppCelular() {
  if (appWifiAtivo) iniciarWifiApp();
  else pararWifiApp();
}

int contarClientesWifiApp() {
  int total = 0;
  for (int i = 0; i < APP_MAX_CLIENTES_TCP; i++) {
    if (appTcpClientes[i] && appTcpClientes[i].connected()) total++;
  }
  return total;
}

bool appClienteConectado() {
  return appWifiServidorAtivo && contarClientesWifiApp() > 0;
}

void processarComandoApp(const char* comando) {
  if (strcmp(comando, "CLR") == 0) {
    if (aldlAtivo) {
      enviarLimparErrosECU();
      beep(freqSucesso);
    }
  }
}

void aceitarClientesWifiApp() {
  NetworkClient novo = appTcpServer.accept();
  if (!novo) return;

  for (int i = 0; i < APP_MAX_CLIENTES_TCP; i++) {
    if (!appTcpClientes[i] || !appTcpClientes[i].connected()) {
      appTcpClientes[i].stop();
      appTcpClientes[i] = novo;
      appTcpClientes[i].setNoDelay(true);
      appTcpBufferLen[i] = 0;
      return;
    }
  }

  // Sem vaga livre
  novo.stop();
}

void lerComandosWifiApp() {
  for (int i = 0; i < APP_MAX_CLIENTES_TCP; i++) {
    if (!appTcpClientes[i]) continue;

    while (appTcpClientes[i].available() > 0) {
      int c = appTcpClientes[i].read();
      if (c < 0) break;

      if (c == '\n' || c == '\r') {
        if (appTcpBufferLen[i] > 0) {
          appTcpBuffer[i][appTcpBufferLen[i]] = '\0';
          processarComandoApp(appTcpBuffer[i]);
          appTcpBufferLen[i] = 0;
        }
      } else if (appTcpBufferLen[i] < sizeof(appTcpBuffer[i]) - 1) {
        appTcpBuffer[i][appTcpBufferLen[i]++] = (char)c;
      }
    }
  }
}

size_t montarJsonApp(char* buffer, size_t tamanho) {
  bool temDados = aldlTemFrameValido && pacotesRecebidos > 0;
  bool fan = atualizarFanEstimado();

  int n = snprintf(buffer, tamanho,
    "{\"v\":1,\"t\":%lu,\"aldl\":%d,\"ecu\":%d,\"ok\":%d,"
    "\"rpm\":%d,\"tps\":%.1f,\"map\":%.2f,\"cts\":%.1f,\"iat\":%.1f,"
    "\"vbat\":%.1f,\"vel\":%d,\"iac\":%d,\"inj\":%.2f,\"co2\":%.2f,"
    "\"spk\":%.1f,\"afr\":%.1f,\"fan\":%d,\"tmot\":%lu,"
    "\"m1\":%u,\"m2\":%u,\"m3\":%u,\"pkt\":%d,\"cks\":%d,"
    "\"id\":%u,\"mod\":\"%s\","
    "\"lt\":%.1f,\"lv\":%.1f,\"lr\":%d}\n",
    (unsigned long)millis(),
    aldlAtivo ? 1 : 0,
    ecuConectada ? 1 : 0,
    temDados ? 1 : 0,
    valorRPM, valorTPS, valorMAP, tempMotor, tempAdmissao,
    voltagem, velocidade, valorIAC, tempoInjecao, voltCO2,
    valorAvancoIgnicao, valorAFR, fan ? 1 : 0, (unsigned long)tempoMotorLigado,
    malf1, malf2, malf3, pacotesRecebidos, errosChecksum,
    ecuIdRaw, ecuModuloCodigo.c_str(),
    alertaTempMotorAtivo ? alertaTempMotorLimite : 0.0f,
    alertaTensaoAtivo ? alertaTensaoMinima : 0.0f,
    alertaShiftLightAtivo ? alertaShiftLightRPM : 0
  );

  if (n < 0) return 0;
  if ((size_t)n >= tamanho) return tamanho - 1;
  return (size_t)n;
}

void enviarLinhaWifiApp(const char* linha, size_t len) {
  for (int i = 0; i < APP_MAX_CLIENTES_TCP; i++) {
    if (!appTcpClientes[i]) continue;

    if (!appTcpClientes[i].connected()) {
      appTcpClientes[i].stop();
      continue;
    }

    size_t escrito = appTcpClientes[i].write((const uint8_t*)linha, len);
    if (escrito != len) {
      // Celular parou de ler (ou saiu da rede): libera a vaga
      appTcpClientes[i].stop();
    }
  }
}

void loopAppCelular() {
  if (!appWifiServidorAtivo) return;

  aceitarClientesWifiApp();
  lerComandosWifiApp();

  if (millis() - appUltimoEnvio < APP_INTERVALO_ENVIO_MS) return;
  appUltimoEnvio = millis();

  if (!appClienteConectado()) return;

  char linha[512];
  size_t len = montarJsonApp(linha, sizeof(linha));
  if (len == 0) return;

  enviarLinhaWifiApp(linha, len);
}

// ======================================================
// TELA CONFIGURACAO > APP CELULAR
// ======================================================

void telaAppCelular() {
  static bool iniciado = false;
  static int item = 0;
  static int ultimoItem = -1;
  static int ultimosClientes = -1;
  static bool ultimoWifiAtivo = false;

  const int totalItens = 2;

  if (!iniciado || forcarRedesenhoTela) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds("APP CELULAR", 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((280 - w) / 2, 12);
    tft.print("APP CELULAR");
    tft.drawFastHLine(30, 36, 220, ST77XX_GREY);

    iniciado = true;
    ultimoItem = -1;
    ultimosClientes = -1;
    ultimoWifiAtivo = !appWifiAtivo;
  }

  if (movimentoEncoder != 0) {
    item += movimentoEncoder;
    if (item < 0) item = totalItens - 1;
    if (item >= totalItens) item = 0;
    movimentoEncoder = 0;
    beep(freqEncoder);
  }

  if (cliqueDetectado) {
    cliqueDetectado = false;

    if (item == 0) {
      appWifiAtivo = !appWifiAtivo;
      aplicarConfigAppCelular();
      salvarConfiguracoes();
      beep(freqClique);
    } else {
      iniciado = false;
      item = 0;
      estadoUI = MENU_UI;
      tft.fillScreen(ST77XX_BLACK);
      desenharMenu();
      beep(freqClique);
      return;
    }
  }

  if (ultimoItem != item || ultimoWifiAtivo != appWifiAtivo) {
    tft.fillRect(0, 44, 280, 52, ST77XX_BLACK);
    desenharLinhaLogOpcao(48, "Wi-Fi para o app", appWifiAtivo, item == 0);
    desenharLinhaLogTexto(72, "Voltar", "", item == 1);

    ultimoItem = item;
    ultimoWifiAtivo = appWifiAtivo;
    ultimosClientes = -1;
  }

  int clientes = contarClientesWifiApp();

  if (clientes != ultimosClientes) {
    tft.fillRect(0, 100, 280, 140, ST77XX_BLACK);

    if (appWifiAtivo && apLigado) {
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(18, 108);
      tft.print("Rede:");
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.setCursor(90, 108);
      tft.print(otaSSID);

      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(18, 132);
      tft.print("Senha:");
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.setCursor(90, 132);
      tft.print(otaSenha);

      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(18, 156);
      tft.print("IP:");
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.setCursor(90, 156);
      tft.print(otaIP);

      tft.setTextSize(2);
      tft.setCursor(18, 186);
      tft.setTextColor(clientes > 0 ? ST77XX_GREEN : ST77XX_GREY, ST77XX_BLACK);
      if (clientes > 0) {
        tft.print("Celular conectado");
      } else {
        tft.print("Aguardando app...");
      }
    } else {
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft.setCursor(18, 120);
      tft.print("Wi-Fi desligado");
    }

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
    tft.setCursor(18, 222);
    tft.print("Com o app conectado a ECU e lida");
    tft.setCursor(18, 232);
    tft.print("mesmo fora das telas de dados.");

    ultimosClientes = clientes;
  }
}

import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'dados_aldl.dart';

enum EstadoConexao { desconectado, conectando, conectado }

/// Conexão TCP com o painel (ESP32 em modo AP, padrão 192.168.4.1:3333).
///
/// O painel envia uma linha JSON por pacote. Se a conexão cair, o app
/// tenta reconectar sozinho enquanto o usuário quiser estar conectado.
class ConexaoPainel extends ChangeNotifier {
  static const _canalRede = MethodChannel('monzadash/rede');
  static const _timeoutSemDados = Duration(seconds: 4);
  static const _intervaloReconexao = Duration(seconds: 2);
  static const tamanhoHistorico = 150; // 30 s a 5 Hz

  String host = '192.168.4.1';
  int porta = 3333;

  EstadoConexao estado = EstadoConexao.desconectado;
  String mensagem = 'Toque em conectar';
  DadosAldl? dados;
  DateTime? ultimaLeitura;
  final List<int> historicoRpm = [];

  bool _querConectado = false;
  Socket? _socket;
  StreamSubscription<String>? _assinatura;
  Timer? _vigia;
  Timer? _reconexao;

  bool get querConectado => _querConectado;

  Future<void> carregarPreferencias() async {
    final prefs = await SharedPreferences.getInstance();
    host = prefs.getString('host') ?? host;
    porta = prefs.getInt('porta') ?? porta;
    notifyListeners();
  }

  Future<void> salvarEndereco(String novoHost, int novaPorta) async {
    host = novoHost.trim();
    porta = novaPorta;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('host', host);
    await prefs.setInt('porta', porta);
    notifyListeners();

    if (_querConectado) {
      await _fecharSocket();
      _tentarConectar();
    }
  }

  void conectar() {
    if (_querConectado) return;
    _querConectado = true;
    _tentarConectar();
  }

  Future<void> desconectar() async {
    _querConectado = false;
    _reconexao?.cancel();
    await _fecharSocket();
    await _desvincularWifi();
    estado = EstadoConexao.desconectado;
    mensagem = 'Desconectado';
    notifyListeners();
  }

  /// Envia o comando de limpar os códigos de erro da ECU.
  bool limparErrosEcu() => _enviar('CLR');

  bool _enviar(String comando) {
    final s = _socket;
    if (s == null || estado != EstadoConexao.conectado) return false;
    try {
      s.write('$comando\n');
      return true;
    } catch (_) {
      return false;
    }
  }

  Future<void> _tentarConectar() async {
    if (!_querConectado || estado == EstadoConexao.conectando) return;

    estado = EstadoConexao.conectando;
    mensagem = 'Conectando em $host:$porta...';
    notifyListeners();

    // Sem isso o Android manda o tráfego pelo 4G quando a rede do
    // painel não tem internet.
    await _vincularWifi();

    try {
      final socket = await Socket.connect(host, porta, timeout: const Duration(seconds: 5));
      if (!_querConectado) {
        socket.destroy();
        return;
      }

      socket.setOption(SocketOption.tcpNoDelay, true);
      _socket = socket;
      estado = EstadoConexao.conectado;
      mensagem = 'Conectado ao painel';
      ultimaLeitura = DateTime.now();
      notifyListeners();

      _assinatura = socket
          .cast<List<int>>()
          .transform(const Utf8Decoder(allowMalformed: true))
          .transform(const LineSplitter())
          .listen(_processarLinha, onError: (_) => _perdeuConexao('Conexão perdida'), onDone: () => _perdeuConexao('Painel encerrou a conexão'), cancelOnError: true);

      _vigia?.cancel();
      _vigia = Timer.periodic(const Duration(seconds: 1), (_) {
        final ultima = ultimaLeitura;
        if (ultima != null && DateTime.now().difference(ultima) > _timeoutSemDados) {
          _perdeuConexao('Sem dados do painel');
        }
      });
    } catch (_) {
      _perdeuConexao('Painel não encontrado. Conecte o celular na rede MonzaDash-OTA.');
    }
  }

  void _processarLinha(String linha) {
    if (linha.isEmpty) return;
    try {
      final json = jsonDecode(linha);
      if (json is! Map<String, dynamic>) return;
      final novo = DadosAldl.fromJson(json);

      dados = novo;
      ultimaLeitura = DateTime.now();
      historicoRpm.add(novo.rpm);
      if (historicoRpm.length > tamanhoHistorico) historicoRpm.removeAt(0);
      notifyListeners();
    } catch (_) {
      // Linha corrompida: ignora e espera a próxima
    }
  }

  Future<void> _perdeuConexao(String motivo) async {
    await _fecharSocket();
    if (!_querConectado) return;

    estado = EstadoConexao.desconectado;
    mensagem = '$motivo. Tentando de novo...';
    notifyListeners();

    _reconexao?.cancel();
    _reconexao = Timer(_intervaloReconexao, _tentarConectar);
  }

  Future<void> _fecharSocket() async {
    _vigia?.cancel();
    _vigia = null;
    await _assinatura?.cancel();
    _assinatura = null;
    _socket?.destroy();
    _socket = null;
    if (estado == EstadoConexao.conectado) estado = EstadoConexao.desconectado;
  }

  Future<void> _vincularWifi() async {
    try {
      await _canalRede.invokeMethod<bool>('vincularWifi');
    } catch (_) {}
  }

  Future<void> _desvincularWifi() async {
    try {
      await _canalRede.invokeMethod<bool>('desvincularWifi');
    } catch (_) {}
  }

  @override
  void dispose() {
    _querConectado = false;
    _reconexao?.cancel();
    _fecharSocket();
    super.dispose();
  }
}

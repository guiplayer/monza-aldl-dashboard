import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'conexao_painel.dart';
import 'dados_aldl.dart';

const _corAlerta = Color(0xFFFF5252);
const _corOk = Color(0xFF4CAF50);
const _corNeutra = Color(0xFF8A9199);

// ======================================================
// CONEXÃO
// ======================================================

class BotaoConexao extends StatelessWidget {
  final ConexaoPainel conexao;

  const BotaoConexao({super.key, required this.conexao});

  @override
  Widget build(BuildContext context) {
    if (conexao.querConectado) {
      return OutlinedButton.icon(
        onPressed: conexao.desconectar,
        icon: const Icon(Icons.link_off, size: 18),
        label: const Text('Desconectar'),
      );
    }
    return FilledButton.icon(
      onPressed: conexao.conectar,
      icon: const Icon(Icons.wifi, size: 18),
      label: const Text('Conectar'),
    );
  }
}

class FaixaStatusConexao extends StatelessWidget {
  final ConexaoPainel conexao;

  const FaixaStatusConexao({super.key, required this.conexao});

  @override
  Widget build(BuildContext context) {
    final dados = conexao.dados;
    final (cor, icone) = switch (conexao.estado) {
      EstadoConexao.conectado => (_corOk, Icons.wifi),
      EstadoConexao.conectando => (Colors.amber, Icons.wifi_find),
      EstadoConexao.desconectado => (_corNeutra, Icons.wifi_off),
    };

    var texto = conexao.mensagem;
    if (conexao.estado == EstadoConexao.conectado && dados != null) {
      texto = dados.ecuOnline ? 'ECU online · ${dados.pacotes} pacotes' : 'Painel conectado · aguardando ECU (ligue a ignição)';
    }

    return Container(
      width: double.infinity,
      color: cor.withValues(alpha: 0.12),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Row(
        children: [
          Icon(icone, size: 18, color: cor),
          const SizedBox(width: 8),
          Expanded(child: Text(texto, style: TextStyle(color: cor, fontSize: 13))),
        ],
      ),
    );
  }
}

Future<void> mostrarConfiguracaoEndereco(BuildContext context, ConexaoPainel conexao) async {
  final hostCtrl = TextEditingController(text: conexao.host);
  final portaCtrl = TextEditingController(text: conexao.porta.toString());

  final salvar = await showDialog<bool>(
    context: context,
    builder: (context) => AlertDialog(
      title: const Text('Endereço do painel'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Text(
            'Conecte o celular na rede Wi-Fi do painel (MonzaDash-OTA, senha 12345678). '
            'Se o Android avisar que a rede não tem internet, escolha manter conectado.',
            style: TextStyle(fontSize: 13),
          ),
          const SizedBox(height: 16),
          TextField(controller: hostCtrl, decoration: const InputDecoration(labelText: 'IP')),
          TextField(
            controller: portaCtrl,
            decoration: const InputDecoration(labelText: 'Porta'),
            keyboardType: TextInputType.number,
          ),
        ],
      ),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancelar')),
        FilledButton(onPressed: () => Navigator.pop(context, true), child: const Text('Salvar')),
      ],
    ),
  );

  if (salvar == true) {
    final porta = int.tryParse(portaCtrl.text) ?? conexao.porta;
    await conexao.salvarEndereco(hostCtrl.text, porta);
  }
}

class _SemDados extends StatelessWidget {
  final ConexaoPainel conexao;

  const _SemDados({required this.conexao});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.directions_car_outlined, size: 64, color: _corNeutra),
            const SizedBox(height: 16),
            const Text(
              '1. Ligue o painel (Configuração > App Celular com Wi-Fi ON)\n'
              '2. Conecte o celular na rede MonzaDash-OTA\n'
              '3. Toque em Conectar',
              style: TextStyle(color: _corNeutra, height: 1.6),
            ),
            const SizedBox(height: 24),
            if (!conexao.querConectado)
              FilledButton.icon(
                onPressed: conexao.conectar,
                icon: const Icon(Icons.wifi),
                label: const Text('Conectar'),
              ),
          ],
        ),
      ),
    );
  }
}

// ======================================================
// ABA PAINEL
// ======================================================

class AbaPainel extends StatelessWidget {
  final ConexaoPainel conexao;

  const AbaPainel({super.key, required this.conexao});

  @override
  Widget build(BuildContext context) {
    final d = conexao.dados;
    if (d == null) return _SemDados(conexao: conexao);

    final valido = d.temDados;
    String f(num v, int casas) => valido ? v.toStringAsFixed(casas) : '--';

    final cards = <_InfoCard>[
      _InfoCard('Temp. motor', f(d.cts, 1), '°C', alerta: d.alertaTemp),
      _InfoCard('Bateria', f(d.vbat, 1), 'V', alerta: d.alertaTensao),
      _InfoCard('Velocidade', valido ? '${d.velocidade}' : '--', 'km/h'),
      _InfoCard('TPS', f(d.tps, 1), '%'),
      _InfoCard('MAP', f(d.map, 2), 'V'),
      _InfoCard('Temp. admissão', f(d.iat, 1), '°C'),
      _InfoCard('Avanço', f(d.spk, 1), '°'),
      _InfoCard('AFR desejado', f(d.afr, 1), ''),
      _InfoCard('Injeção', f(d.injecao, 2), 'ms'),
      _InfoCard('IAC', valido ? '${d.iac}' : '--', 'passos'),
      _InfoCard('CO2 POT', f(d.co2, 2), 'V'),
      _InfoCard('Ventoinha', valido ? (d.fan ? 'ON' : 'OFF') : '--', 'estimada', destaque: d.fan),
      _InfoCard('Motor ligado', valido ? d.tempoMotorFormatado : '--', ''),
    ];

    return LayoutBuilder(
      builder: (context, constraints) {
        final colunas = constraints.maxWidth > 700 ? 4 : (constraints.maxWidth > 420 ? 3 : 2);
        return CustomScrollView(
          slivers: [
            SliverPadding(
              padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
              sliver: SliverToBoxAdapter(
                child: _CardRpm(dados: d, historico: conexao.historicoRpm),
              ),
            ),
            SliverPadding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 16),
              sliver: SliverGrid.count(
                crossAxisCount: colunas,
                mainAxisSpacing: 10,
                crossAxisSpacing: 10,
                childAspectRatio: 1.45,
                children: cards,
              ),
            ),
          ],
        );
      },
    );
  }
}

class _InfoCard extends StatelessWidget {
  final String titulo;
  final String valor;
  final String unidade;
  final bool alerta;
  final bool destaque;

  const _InfoCard(this.titulo, this.valor, this.unidade, {this.alerta = false, this.destaque = false});

  @override
  Widget build(BuildContext context) {
    final esquema = Theme.of(context).colorScheme;
    final corValor = alerta ? _corAlerta : (destaque ? _corOk : esquema.onSurface);

    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: alerta ? _corAlerta.withValues(alpha: 0.15) : esquema.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: alerta ? _corAlerta : Colors.transparent, width: 1.5),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(titulo, style: const TextStyle(fontSize: 12, color: _corNeutra)),
          FittedBox(
            fit: BoxFit.scaleDown,
            alignment: Alignment.centerLeft,
            child: Text(
              valor,
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.w600,
                color: corValor,
                fontFeatures: const [FontFeature.tabularFigures()],
              ),
            ),
          ),
          Text(unidade, style: const TextStyle(fontSize: 12, color: _corNeutra)),
        ],
      ),
    );
  }
}

class _CardRpm extends StatelessWidget {
  final DadosAldl dados;
  final List<int> historico;

  const _CardRpm({required this.dados, required this.historico});

  @override
  Widget build(BuildContext context) {
    final esquema = Theme.of(context).colorScheme;
    final limite = dados.limiteRpm > 0 ? dados.limiteRpm : 6000;
    final maximo = math.max(7000, limite + 1000).toDouble();
    final alerta = dados.alertaRpm;
    final cor = alerta ? _corAlerta : esquema.primary;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: alerta ? _corAlerta.withValues(alpha: 0.15) : esquema.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: alerta ? _corAlerta : Colors.transparent, width: 1.5),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              Text(
                dados.temDados ? '${dados.rpm}' : '--',
                style: TextStyle(
                  fontSize: 56,
                  height: 1,
                  fontWeight: FontWeight.w700,
                  color: cor,
                  fontFeatures: const [FontFeature.tabularFigures()],
                ),
              ),
              const SizedBox(width: 8),
              const Padding(
                padding: EdgeInsets.only(bottom: 6),
                child: Text('RPM', style: TextStyle(color: _corNeutra)),
              ),
              const Spacer(),
              if (alerta)
                const Padding(
                  padding: EdgeInsets.only(bottom: 6),
                  child: Text('SHIFT!', style: TextStyle(color: _corAlerta, fontWeight: FontWeight.w700)),
                ),
            ],
          ),
          const SizedBox(height: 12),
          ClipRRect(
            borderRadius: BorderRadius.circular(6),
            child: LinearProgressIndicator(
              value: (dados.rpm / maximo).clamp(0.0, 1.0),
              minHeight: 10,
              color: cor,
              backgroundColor: esquema.surfaceContainerHighest,
            ),
          ),
          const SizedBox(height: 12),
          SizedBox(
            height: 70,
            width: double.infinity,
            child: CustomPaint(
              painter: _GraficoRpm(historico, maximo, cor, limite.toDouble()),
            ),
          ),
          const Text('Últimos 30 s', style: TextStyle(fontSize: 11, color: _corNeutra)),
        ],
      ),
    );
  }
}

class _GraficoRpm extends CustomPainter {
  final List<int> valores;
  final double maximo;
  final Color cor;
  final double limite;

  _GraficoRpm(List<int> valores, this.maximo, this.cor, this.limite) : valores = List.of(valores);

  @override
  void paint(Canvas canvas, Size size) {
    final yLimite = size.height - (limite / maximo) * size.height;
    final tracejado = Paint()
      ..color = _corAlerta.withValues(alpha: 0.5)
      ..strokeWidth = 1;
    for (double x = 0; x < size.width; x += 8) {
      canvas.drawLine(Offset(x, yLimite), Offset(x + 4, yLimite), tracejado);
    }

    if (valores.length < 2) return;

    final passo = size.width / (ConexaoPainel.tamanhoHistorico - 1);
    final inicio = size.width - passo * (valores.length - 1);
    final caminho = Path();
    for (var i = 0; i < valores.length; i++) {
      final x = inicio + passo * i;
      final y = size.height - (valores[i] / maximo).clamp(0.0, 1.0) * size.height;
      if (i == 0) {
        caminho.moveTo(x, y);
      } else {
        caminho.lineTo(x, y);
      }
    }

    canvas.drawPath(
      caminho,
      Paint()
        ..color = cor
        ..strokeWidth = 2
        ..style = PaintingStyle.stroke
        ..strokeJoin = StrokeJoin.round,
    );
  }

  @override
  bool shouldRepaint(_GraficoRpm old) => true;
}

// ======================================================
// ABA CÓDIGOS
// ======================================================

class AbaCodigos extends StatelessWidget {
  final ConexaoPainel conexao;

  const AbaCodigos({super.key, required this.conexao});

  Future<void> _confirmarLimpeza(BuildContext context) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Limpar erros da ECU?'),
        content: const Text('Use com a ignição ligada. Depois confira se os códigos voltaram.'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancelar')),
          FilledButton(onPressed: () => Navigator.pop(context, true), child: const Text('Limpar')),
        ],
      ),
    );
    if (ok != true || !context.mounted) return;

    final enviado = conexao.limparErrosEcu();
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(enviado ? 'Comando enviado para a ECU' : 'Não conectado ao painel')),
    );
  }

  @override
  Widget build(BuildContext context) {
    final d = conexao.dados;
    if (d == null) return _SemDados(conexao: conexao);

    final falhas = d.falhasAtivas;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        if (!d.temDados)
          const _Aviso('Aguardando leitura da ECU...', _corNeutra, Icons.hourglass_empty)
        else if (falhas.isEmpty)
          const _Aviso('Sem falhas', _corOk, Icons.check_circle_outline)
        else
          ...falhas.map((f) => Card(
                color: _corAlerta.withValues(alpha: 0.12),
                child: ListTile(
                  leading: const Icon(Icons.error_outline, color: _corAlerta),
                  title: Text('Erro $f'),
                ),
              )),
        const SizedBox(height: 24),
        OutlinedButton.icon(
          onPressed: d.ecuOnline ? () => _confirmarLimpeza(context) : null,
          icon: const Icon(Icons.cleaning_services_outlined),
          label: const Text('Limpar erros da ECU'),
        ),
      ],
    );
  }
}

class _Aviso extends StatelessWidget {
  final String texto;
  final Color cor;
  final IconData icone;

  const _Aviso(this.texto, this.cor, this.icone);

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 32),
      decoration: BoxDecoration(
        border: Border.all(color: cor),
        borderRadius: BorderRadius.circular(14),
      ),
      child: Column(
        children: [
          Icon(icone, color: cor, size: 40),
          const SizedBox(height: 8),
          Text(texto, style: TextStyle(color: cor, fontSize: 20, fontWeight: FontWeight.w600)),
        ],
      ),
    );
  }
}

// ======================================================
// ABA STATUS
// ======================================================

class AbaStatus extends StatelessWidget {
  final ConexaoPainel conexao;

  const AbaStatus({super.key, required this.conexao});

  @override
  Widget build(BuildContext context) {
    final d = conexao.dados;
    if (d == null) return _SemDados(conexao: conexao);

    Widget linha(String nome, String valor, {Color? cor}) => ListTile(
          dense: true,
          title: Text(nome),
          trailing: Text(valor, style: TextStyle(fontSize: 15, color: cor, fontWeight: FontWeight.w600)),
        );

    return ListView(
      padding: const EdgeInsets.symmetric(vertical: 8),
      children: [
        linha('Painel', '${conexao.host}:${conexao.porta}'),
        linha('Leitura ALDL', d.aldlAtivo ? 'ATIVA' : 'PARADA', cor: d.aldlAtivo ? _corOk : _corNeutra),
        linha('ECU', d.ecuOnline ? 'ONLINE' : 'OFFLINE', cor: d.ecuOnline ? _corOk : _corAlerta),
        linha('Pacotes recebidos', '${d.pacotes}'),
        linha('Erros de checksum', '${d.errosChecksum}', cor: d.errosChecksum > 0 ? Colors.amber : null),
        const Divider(),
        linha('ECU ID', d.ecuId > 0 ? '${d.ecuId} (0x${d.ecuId.toRadixString(16).toUpperCase()})' : '--'),
        linha('Módulo', d.modulo.isEmpty ? '--' : d.modulo),
        const Divider(),
        linha('Alerta temperatura', d.limiteTemp > 0 ? '${d.limiteTemp.toStringAsFixed(0)} °C' : 'desligado'),
        linha('Alerta bateria', d.limiteTensao > 0 ? '${d.limiteTensao.toStringAsFixed(1)} V' : 'desligado'),
        linha('Shift light', d.limiteRpm > 0 ? '${d.limiteRpm} RPM' : 'desligado'),
      ],
    );
  }
}

/// Pacote de telemetria enviado pelo painel (firmware app_celular.ino).
class DadosAldl {
  final int millis;
  final bool aldlAtivo;
  final bool ecuOnline;
  final bool temDados;

  final int rpm;
  final double tps;
  final double map;
  final double cts;
  final double iat;
  final double vbat;
  final int velocidade;
  final int iac;
  final double injecao;
  final double co2;
  final double spk;
  final double afr;
  final bool fan;
  final int tempoMotor;

  final int malf1;
  final int malf2;
  final int malf3;
  final int pacotes;
  final int errosChecksum;
  final int ecuId;
  final String modulo;

  /// Limites de alerta configurados no painel (0 = alerta desligado).
  final double limiteTemp;
  final double limiteTensao;
  final int limiteRpm;

  const DadosAldl({
    required this.millis,
    required this.aldlAtivo,
    required this.ecuOnline,
    required this.temDados,
    required this.rpm,
    required this.tps,
    required this.map,
    required this.cts,
    required this.iat,
    required this.vbat,
    required this.velocidade,
    required this.iac,
    required this.injecao,
    required this.co2,
    required this.spk,
    required this.afr,
    required this.fan,
    required this.tempoMotor,
    required this.malf1,
    required this.malf2,
    required this.malf3,
    required this.pacotes,
    required this.errosChecksum,
    required this.ecuId,
    required this.modulo,
    required this.limiteTemp,
    required this.limiteTensao,
    required this.limiteRpm,
  });

  factory DadosAldl.fromJson(Map<String, dynamic> j) {
    int i(String k) => (j[k] as num?)?.toInt() ?? 0;
    double d(String k) => (j[k] as num?)?.toDouble() ?? 0.0;
    bool b(String k) => i(k) != 0;

    return DadosAldl(
      millis: i('t'),
      aldlAtivo: b('aldl'),
      ecuOnline: b('ecu'),
      temDados: b('ok'),
      rpm: i('rpm'),
      tps: d('tps'),
      map: d('map'),
      cts: d('cts'),
      iat: d('iat'),
      vbat: d('vbat'),
      velocidade: i('vel'),
      iac: i('iac'),
      injecao: d('inj'),
      co2: d('co2'),
      spk: d('spk'),
      afr: d('afr'),
      fan: b('fan'),
      tempoMotor: i('tmot'),
      malf1: i('m1'),
      malf2: i('m2'),
      malf3: i('m3'),
      pacotes: i('pkt'),
      errosChecksum: i('cks'),
      ecuId: i('id'),
      modulo: (j['mod'] as String?) ?? '',
      limiteTemp: d('lt'),
      limiteTensao: d('lv'),
      limiteRpm: i('lr'),
    );
  }

  bool get alertaTemp => temDados && limiteTemp > 0 && cts >= limiteTemp;
  bool get alertaTensao => temDados && limiteTensao > 0 && vbat > 0 && vbat < limiteTensao;
  bool get alertaRpm => temDados && limiteRpm > 0 && rpm >= limiteRpm;

  String get tempoMotorFormatado {
    final h = tempoMotor ~/ 3600;
    final m = (tempoMotor % 3600) ~/ 60;
    final s = tempoMotor % 60;
    String dois(int v) => v.toString().padLeft(2, '0');
    return h > 0 ? '${dois(h)}:${dois(m)}:${dois(s)}' : '${dois(m)}:${dois(s)}';
  }

  /// Mesma tabela de obterFalhasAtivas() do firmware.
  List<String> get falhasAtivas {
    final falhas = <String>[];
    bool bit(int v, int n) => (v & (1 << n)) != 0;

    if (bit(malf1, 0)) falhas.add('24 - Sem sinal sensor velocidade');
    if (bit(malf1, 2)) falhas.add('22 - TPS voltagem baixa');
    if (bit(malf1, 3)) falhas.add('21 - TPS voltagem alta');
    if (bit(malf1, 4)) falhas.add('15 - Sensor temperatura / chicote');
    if (bit(malf1, 5)) falhas.add('14 - Sensor temperatura / curto massa');
    if (bit(malf1, 7)) falhas.add('12 - Sem pulsos referência');

    if (bit(malf2, 0)) falhas.add('42 - Módulo HEI');
    if (bit(malf2, 2)) falhas.add('35 - Atuador IAC');
    if (bit(malf2, 3)) falhas.add('34 - MAP sinal baixo');
    if (bit(malf2, 4)) falhas.add('33 - MAP sinal alto');
    if (bit(malf2, 5)) falhas.add('32 - Falha sistema EGR');

    if (bit(malf3, 0)) falhas.add('55 - Unidade comando defeito');
    if (bit(malf3, 1)) falhas.add('54 - Potenciômetro CO2');
    if (bit(malf3, 4)) falhas.add('51 - Unidade comando / MEM-CAL');

    return falhas;
  }
}

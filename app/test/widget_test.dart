import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:monza_aldl_app/dados_aldl.dart';

void main() {
  // Linha no mesmo formato de montarJsonApp() do firmware
  const linha = '{"v":1,"t":123456,"aldl":1,"ecu":1,"ok":1,'
      '"rpm":5600,"tps":12.5,"map":1.28,"cts":106.5,"iat":30.0,'
      '"vbat":11.2,"vel":42,"iac":34,"inj":2.15,"co2":1.82,'
      '"spk":12.3,"afr":14.7,"fan":1,"tmot":3725,'
      '"m1":129,"m2":4,"m3":0,"pkt":321,"cks":2,'
      '"id":5999,"mod":"AYMM","lt":105.0,"lv":11.5,"lr":5500}';

  test('interpreta o pacote do painel', () {
    final d = DadosAldl.fromJson(jsonDecode(linha) as Map<String, dynamic>);

    expect(d.rpm, 5600);
    expect(d.cts, 106.5);
    expect(d.fan, isTrue);
    expect(d.modulo, 'AYMM');
    expect(d.tempoMotorFormatado, '01:02:05');
  });

  test('alertas usam os limites do painel', () {
    final d = DadosAldl.fromJson(jsonDecode(linha) as Map<String, dynamic>);

    expect(d.alertaTemp, isTrue);
    expect(d.alertaTensao, isTrue);
    expect(d.alertaRpm, isTrue);
  });

  test('decodifica códigos de falha', () {
    final d = DadosAldl.fromJson(jsonDecode(linha) as Map<String, dynamic>);

    expect(d.falhasAtivas, [
      '24 - Sem sinal sensor velocidade',
      '12 - Sem pulsos referência',
      '35 - Atuador IAC',
    ]);
  });
}

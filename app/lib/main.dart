import 'package:flutter/material.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

import 'conexao_painel.dart';
import 'telas.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  final conexao = ConexaoPainel()..carregarPreferencias();
  runApp(MonzaAldlApp(conexao: conexao));
}

class MonzaAldlApp extends StatelessWidget {
  final ConexaoPainel conexao;

  const MonzaAldlApp({super.key, required this.conexao});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Monza ALDL',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        brightness: Brightness.dark,
        colorSchemeSeed: Colors.cyan,
        scaffoldBackgroundColor: const Color(0xFF0B0D10),
      ),
      home: TelaPrincipal(conexao: conexao),
    );
  }
}

class TelaPrincipal extends StatefulWidget {
  final ConexaoPainel conexao;

  const TelaPrincipal({super.key, required this.conexao});

  @override
  State<TelaPrincipal> createState() => _TelaPrincipalState();
}

class _TelaPrincipalState extends State<TelaPrincipal> {
  int _aba = 0;

  ConexaoPainel get conexao => widget.conexao;

  @override
  void initState() {
    super.initState();
    conexao.addListener(_atualizarWakelock);
  }

  @override
  void dispose() {
    conexao.removeListener(_atualizarWakelock);
    super.dispose();
  }

  // Mantém a tela acesa enquanto estiver monitorando o carro
  void _atualizarWakelock() {
    WakelockPlus.toggle(enable: conexao.querConectado);
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: conexao,
      builder: (context, _) {
        return Scaffold(
          appBar: AppBar(
            title: const Text('Monza ALDL'),
            actions: [
              IconButton(
                tooltip: 'Endereço do painel',
                icon: const Icon(Icons.settings_outlined),
                onPressed: () => mostrarConfiguracaoEndereco(context, conexao),
              ),
              Padding(
                padding: const EdgeInsets.only(right: 8),
                child: BotaoConexao(conexao: conexao),
              ),
            ],
          ),
          body: Column(
            children: [
              FaixaStatusConexao(conexao: conexao),
              Expanded(
                child: switch (_aba) {
                  0 => AbaPainel(conexao: conexao),
                  1 => AbaCodigos(conexao: conexao),
                  _ => AbaStatus(conexao: conexao),
                },
              ),
            ],
          ),
          bottomNavigationBar: NavigationBar(
            selectedIndex: _aba,
            onDestinationSelected: (i) => setState(() => _aba = i),
            destinations: const [
              NavigationDestination(icon: Icon(Icons.speed), label: 'Painel'),
              NavigationDestination(icon: Icon(Icons.warning_amber), label: 'Códigos'),
              NavigationDestination(icon: Icon(Icons.memory), label: 'Status'),
            ],
          ),
        );
      },
    );
  }
}

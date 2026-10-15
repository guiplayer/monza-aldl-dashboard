# Monza ALDL - App Android

App Flutter que mostra no celular os dados ALDL lidos pelo painel, via Wi-Fi.

## Compilar

```bash
flutter pub get
flutter build apk --release
```

O APK fica em `build/app/outputs/flutter-apk/app-release.apk`.

Ao criar uma tag `v*` no GitHub, o workflow `Compilar firmware` também gera o APK e anexa no Release.

## Protocolo

- O painel cria a rede `MonzaDash-OTA` (senha `12345678`), IP `192.168.4.1`.
- O app abre uma conexão TCP na porta `3333`.
- O painel envia uma linha JSON por pacote (5 por segundo), terminada em `\n`.
- O app pode enviar `CLR\n` para limpar os códigos de erro da ECU.

Campos da linha JSON (montada em `montarJsonApp()` no firmware):

| Campo | Significado |
|---|---|
| `t` | millis() do ESP32 |
| `aldl`, `ecu`, `ok` | leitura ALDL ativa, ECU online, já recebeu frame válido |
| `rpm`, `tps`, `map`, `cts`, `iat`, `vbat`, `vel`, `iac`, `inj`, `co2`, `spk`, `afr` | sensores |
| `fan`, `tmot` | ventoinha estimada, tempo de motor ligado (s) |
| `m1`, `m2`, `m3` | malfunction words (códigos de falha) |
| `pkt`, `cks` | pacotes recebidos, erros de checksum |
| `id`, `mod` | ECU ID e código do módulo |
| `lt`, `lv`, `lr` | limites de alerta do painel (0 = desligado) |

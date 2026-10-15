package br.com.monzadash.monza_aldl_app

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "monzadash/rede")
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "vincularWifi" -> result.success(vincularWifi())
                    "desvincularWifi" -> {
                        conectividade().bindProcessToNetwork(null)
                        result.success(true)
                    }
                    else -> result.notImplemented()
                }
            }
    }

    private fun conectividade() =
        getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

    // A rede do painel nao tem internet, entao o Android prefere o 4G.
    // Amarra o app na rede Wi-Fi para os sockets chegarem no ESP32.
    @Suppress("DEPRECATION")
    private fun vincularWifi(): Boolean {
        val cm = conectividade()
        val wifi = cm.allNetworks.firstOrNull { rede ->
            cm.getNetworkCapabilities(rede)?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
        } ?: return false

        return cm.bindProcessToNetwork(wifi)
    }
}

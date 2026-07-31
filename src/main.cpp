#include "DHT.h"
#include <WiFi.h>
#include <WebServer.h>

// ====================================================================
// DEFINIÇÃO DOS PINOS
// ====================================================================
#define DHTPIN 15
#define LDRPIN 34
#define AC_PIN 12        // Relé 1 (Ar Condicionado)
#define HUMIDIFIER_PIN 14 // Relé 2 (Humidificador)

#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ====================================================================
// REDE WI-FI (Configuração Wokwi) E SERVIDOR
// ====================================================================
const char* ssid = "Wokwi-GUEST";
const char* password = "";
WebServer server(80); // Servidor web na porta padrão 80

// ====================================================================
// MÁQUINA DE ESTADOS E TEMPO
// ====================================================================
enum EstadoProjeto {
  ESTADO_WIFI,
  ESTADO_LEITURA,
  ESTADO_PROCESSAMENTO,
  ESTADO_ALERTA
};

EstadoProjeto estadoAtual = ESTADO_WIFI;
unsigned long tempoAnterior = 0;
const long intervaloLeitura = 2000; 

// ====================================================================
// VARIÁVEIS DO SISTEMA E PARÂMETROS
// ====================================================================
float temperatura = 0.0;
float humidade = 0.0;
int luzAnalogica = 0;
bool acAtivo = false;
bool humidificadorAtivo = false;

const float TEMP_MIN = 20.0;
const float TEMP_MAX = 25.0;
const float HUM_MIN = 40.0;
const float HUM_MAX = 60.0;
const int LIMITE_LUZ = 2500; 
bool pessoasPresentes = true;

// ====================================================================
// PÁGINA HTML (DASHBOARD)
// ====================================================================
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
  html += "<meta http-equiv=\"refresh\" content=\"2\">"; // Atualiza a página a cada 2 seg
  html += "<title>AeroSmart IoT Dashboard</title>";
  html += "<style>body{font-family: Arial; text-align: center; margin-top: 50px; background-color: #f4f4f9;} ";
  html += ".card{background: white; padding: 20px; margin: 10px auto; border-radius: 10px; width: 300px; box-shadow: 0 4px 8px rgba(0,0,0,0.1);}</style></head><body>";
  html += "<h1>AeroSmart IoT Pro</h1>";
  
  html += "<div class=\"card\"><h2>Clima</h2>";
  html += "<p>Temperatura: <strong>" + String(temperatura) + " &deg;C</strong></p>";
  html += "<p>Humidade: <strong>" + String(humidade) + " %</strong></p></div>";

  html += "<div class=\"card\"><h2>Ocupação (Eco Mode)</h2>";
  html += "<p>Status: <strong>" + String(pessoasPresentes ? "Pessoas na Sala" : "Sala Vazia (Eco Ativo)") + "</strong></p></div>";

  html += "<div class=\"card\"><h2>Atuadores</h2>";
  html += "<p>Ar Condicionado: <strong>" + String(acAtivo ? "<span style='color:green;'>LIGADO</span>" : "<span style='color:red;'>DESLIGADO</span>") + "</strong></p>";
  html += "<p>Humidificador: <strong>" + String(humidificadorAtivo ? "<span style='color:green;'>LIGADO</span>" : "<span style='color:red;'>DESLIGADO</span>") + "</strong></p></div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ====================================================================
// SETUP INICIAL
// ====================================================================
void setup() {
  Serial.begin(115200);
  dht.begin();
  
  pinMode(LDRPIN, INPUT); 
  pinMode(AC_PIN, OUTPUT);
  pinMode(HUMIDIFIER_PIN, OUTPUT);
  
  // Garante que os relés começam desligados
  digitalWrite(AC_PIN, LOW);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  
  // Roteamento do Servidor Web
  server.on("/", handleRoot);
}

// ====================================================================
// LOOP PRINCIPAL (MÁQUINA DE ESTADOS)
// ====================================================================
void loop() {
  unsigned long tempoAtual = millis();
  server.handleClient(); // Mantém o servidor Web à escuta

  switch (estadoAtual) {
    
    case ESTADO_WIFI:
      Serial.print("A ligar ao Wi-Fi ");
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("\nWi-Fi Ligado!");
      Serial.print("Endereço IP para o Dashboard: ");
      Serial.println(WiFi.localIP());
      
      server.begin(); // Inicia o servidor web
      estadoAtual = ESTADO_LEITURA;
      break;

    case ESTADO_LEITURA:
      if (tempoAtual - tempoAnterior >= intervaloLeitura) {
        tempoAnterior = tempoAtual;
        
        temperatura = dht.readTemperature();
        humidade = dht.readHumidity();
        luzAnalogica = analogRead(LDRPIN);
        
        estadoAtual = ESTADO_PROCESSAMENTO; 
      }
      break;

    case ESTADO_PROCESSAMENTO: {
      if (isnan(temperatura) || isnan(humidade)) {
        estadoAtual = ESTADO_LEITURA;
        break;
      }

      pessoasPresentes = (luzAnalogica < LIMITE_LUZ);

      if (!pessoasPresentes) {
        acAtivo = false;
        humidificadorAtivo = false;
      } 
      else {
        acAtivo = (temperatura > TEMP_MAX || temperatura < TEMP_MIN);
        humidificadorAtivo = (humidade < HUM_MIN);
      }
      
      estadoAtual = ESTADO_ALERTA; 
      break;
    }

    case ESTADO_ALERTA:
      digitalWrite(AC_PIN, acAtivo ? HIGH : LOW);
      digitalWrite(HUMIDIFIER_PIN, humidificadorAtivo ? HIGH : LOW);
      estadoAtual = ESTADO_LEITURA; 
      break;
  }
}

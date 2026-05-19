#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> // Para enviar datos formateados a la web
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Configuración OpenWeatherMap
const String apiKey = "e41bd5ef197ab7ccdae632db3d023654";
<<<<<<< HEAD
const String city = "Buenos Aires";
const String countryCode = "AR";
=======
const String city = "TuCiudad";
const String countryCode = "AR"; // Ejemplo: AR para Argentina, ES para España
>>>>>>> b2432bbcd17ccdd704f420df52f3e2095c543f1b
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 1800000; // 30 min

// Variables de decisión
bool lloviendo = false;
int humedadAmbiente = 0;

#define BUZZER 8
#define RELE   5
#define SERVO  3
#define TRIG   2
#define ECHO   1  
#define LDR    0

// WiFi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Interfaz web embebida
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Voltix: Jardin Vertical</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    html { font-family: 'Segoe UI', sans-serif; display: inline-block; text-align: center; background-color: #1a1a1a; color: white;}
    h2 { font-size: 2.0rem; color: #4CAF50; margin-bottom: 30px; }
    .content { padding: 20px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 20px; justify-items: center; }
    .card { background-color: #2d2d2d; border-radius: 15px; padding: 20px; width: 100%; max-width: 240px; box-shadow: 0 4px 8px rgba(0,0,0,0.3); }
    
    /* Estilos del Gauge Semicircular */
    .gauge { position: relative; width: 160px; height: 80px; overflow: hidden; margin: 0 auto; }
    .gauge-body { position: absolute; width: 100%; height: 200%; border-radius: 50%; background: #444; box-sizing: border-box; border: 20px solid #444; border-bottom: none; }
    .gauge-fill { position: absolute; top: 0; left: 0; width: 100%; height: 200%; border-radius: 50%; box-sizing: border-box; border: 20px solid #4CAF50; border-bottom: none; transform-origin: center top; transform: rotate(0deg); transition: transform 0.5s ease-out; }
    .gauge-water { border-color: #2196F3; }
    .gauge-light { border-color: #FFC107; }
    .gauge-temp { border-color: #FF5722; }
    .gauge-cover { position: absolute; width: 75%; height: 150%; background: #2d2d2d; border-radius: 50%; top: 25%; left: 12.5%; display: flex; align-items: center; justify-content: center; padding-bottom: 35%; box-sizing: border-box; font-size: 1.2rem; font-weight: bold; }
    
    .status-on { color: #4CAF50; font-weight: bold; }
    .status-off { color: #f44336; font-weight: bold; }
    .btn { background-color: #4CAF50; border: none; color: white; padding: 12px 24px; border-radius: 5px; cursor: pointer; font-size: 1rem; margin-top: 10px; }
    .slider { width: 80%; accent-color: #4CAF50; margin: 15px 0; }
    .meta-text { font-size: 0.9rem; color: #aaa; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="content">
    <h2><i class="fas fa-chart-pie"></i> Panel de Control Voltix</h2>
    
    <div class="grid">
      <!-- Gauge Nivel de Agua -->
      <div class="card">
        <h3><i class="fas fa-tint" style="color:#2196F3"></i> Nivel Agua</h3>
        <div class="gauge">
          <div class="gauge-body"></div>
          <div class="gauge-fill gauge-water" id="gauge_water"></div>
          <div class="gauge-cover" id="text_water">--%</div>
        </div>
      </div>

      <!-- Gauge LDR (Luz) -->
      <div class="card">
        <h3><i class="fas fa-sun" style="color:#FFC107"></i> Intensidad Luz</h3>
        <div class="gauge">
          <div class="gauge-body"></div>
          <div class="gauge-fill gauge-light" id="gauge_light"></div>
          <div class="gauge-cover" id="text_light">--%</div>
        </div>
        <div class="meta-text" id="raw_light">0 lx</div>
      </div>

      <!-- Gauge Temperatura -->
      <div class="card">
        <h3><i class="fas fa-thermometer-half" style="color:#FF5722"></i> Temperatura</h3>
        <div class="gauge">
          <div class="gauge-body"></div>
          <div class="gauge-fill gauge-temp" id="gauge_temp"></div>
          <div class="gauge-cover" id="text_temp">--&deg;C</div>
        </div>
      </div>
    </div>

    <br>
    <div class="grid">
      <div class="card" style="max-width: 100%;">
        <h3>Control de bomba</h3>
        <p>Estado: <span id="pump_state" class="status-off">APAGADO</span></p>
        <button class="btn" onclick="togglePump()">Encendido</button>
      </div>

      <div class="card" style="max-width: 100%;">
        <h3>Rotacion de base</h3>
        <input type="range" min="0" max="180" value="90" class="slider" id="servoPos" oninput="moveBase(this.value)">
        <p>Angulo: <span id="angle_val">90</span>&deg;</p>
      </div>
    </div>
  </div>

<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', initWebSocket);

  function initWebSocket() {
    websocket = new WebSocket(gateway);
    websocket.onmessage = onMessage;
  }

  // Convierte un porcentaje (0-100) en grados de rotación CSS (0 a 0.5turn / 180deg)
  void function setGaugeValue(gaugeId, textId, value, suffix) {
    if(value < 0) value = 0;
    if(value > 100) value = 100;
    
    var rotateDeg = value / 100 * 0.5;
    document.getElementById(gaugeId).style.transform = `rotate(${rotateDeg}turn)`;
    document.getElementById(textId).innerHTML = Math.round(value) + suffix;
  }

  function onMessage(event) {
    var data = JSON.parse(event.data);
    
    // 1. Gauge Agua (Ya viene en porcentaje)
    setGaugeValue('gauge_water', 'text_water', data.water, '%');
    
    // 2. Gauge Luz (El ESP32-C3 lee de 0 a 4095 en el ADC. Lo mapeamos a % para el gráfico)
    var lightPercent = (data.light / 4095) * 100;
    setGaugeValue('gauge_light', 'text_light', lightPercent, '%');
    document.getElementById('raw_light').innerHTML = data.light + " ADC";

    // 3. Gauge Temperatura (Mapeamos un rango lógico de 0°C a 50°C para la aguja)
    var tempPercent = (data.temp / 50) * 100;
    setGaugeValue('gauge_temp', 'text_temp', data.temp, '&deg;C');

    // Estado bomba
    document.getElementById('pump_state').innerHTML = data.pump ? "ENCENDIDO" : "APAGADO";
    document.getElementById('pump_state').className = data.pump ? "status-on" : "status-off";
  }

  function togglePump() { websocket.send('toggle_pump'); }
  function moveBase(val) { 
    document.getElementById('angle_val').innerHTML = val;
    websocket.send('servo:' + val); 
  }
</script>
</body>
</html>
)rawliteral";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Variables de estado
bool bombaActiva = false;
float temperaturaActual = 25.0;
int nivelAgua = 80;

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    
    if (message == "toggle_pump") {
      bombaActiva = !bombaActiva;
      digitalWrite(RELE, bombaActiva);
    }
    if (message.startsWith("servo:")) {
      int angle = message.substring(6).toInt();
      // Aquí mueves tu motor paso a paso o servo
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Cliente WebSocket conectado desde: %s\n", client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Cliente WebSocket desconectado\n");
      break;
    case WS_EVT_DATA:
      // Aquí procesamos los mensajes que llegan desde la interfaz web
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // URL para clima actual
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";
    
    http.begin(url);
    int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    
    // Cambiamos StaticJsonDocument<1024> por JsonDocument
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      humedadAmbiente = doc["main"]["humidity"];
      const char* weatherMain = doc["weather"][0]["main"];
      lloviendo = (strcmp(weatherMain, "Rain") == 0);
      Serial.printf("Clima actualizado: %s, Humedad: %d%%\n", weatherMain, humedadAmbiente);
    }
  }
  http.end();
  }
}

void notifyClients() {
  JsonDocument doc; 
  
  doc["water"] = nivelAgua;
  doc["light"] = analogRead (LDR);
  doc["temp"] = temperaturaActual;
  doc["pump"] = bombaActiva;
  doc["weather"] = lloviendo ? "Lloviendo" : "Despejado/Nubes";
  doc["ext_hum"] = humedadAmbiente;
  
  // La serialización sigue funcionando igual
  String buffer;
  serializeJson(doc, buffer);
  ws.textAll(buffer);
}

void setup() {
  // ... inicialización de pines y sensores ...

  WiFi.begin(ssid, password);
  
  ws.onEvent (onEvent);
  server.addHandler (&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
}

void loop() {
  ws.cleanupClients();
  
  // Actualizar datos cada 2 segundos
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    // Lectura de sensores reales aquí
    notifyClients();
    lastUpdate = millis();
  }
}
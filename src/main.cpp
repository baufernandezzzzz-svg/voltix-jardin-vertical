#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> // Para enviar datos formateados a la web
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Configuración OpenWeatherMap
const String apiKey = "e41bd5ef197ab7ccdae632db3d023654";
const String city = "TuCiudad";
const String countryCode = "AR"; // Ejemplo: AR para Argentina, ES para España
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 1800000; // Consultar cada 30 min (1800000 ms)

// Variables de decisión
bool isRaining = false;
int externalHumidity = 0;

#define LDR    9
#define BUZZER 8
#define RELE   5
#define SERVO  3
#define TRIG   2
#define ECHO   1  

// WiFi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Interfaz web embebida
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>HydroSmart ESP32-C3</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    html { font-family: 'Segoe UI', sans-serif; display: inline-block; text-align: center; background-color: #1a1a1a; color: white;}
    h2 { font-size: 2.0rem; color: #4CAF50; }
    .content { padding: 20px; }
    .card { background-color: #2d2d2d; border-radius: 15px; padding: 15px; margin: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.2); }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; }
    .status-on { color: #4CAF50; font-weight: bold; }
    .status-off { color: #f44336; font-weight: bold; }
    .btn { background-color: #4CAF50; border: none; color: white; padding: 10px 20px; border-radius: 5px; cursor: pointer; }
    .btn-off { background-color: #f44336; }
    .slider { width: 80%; accent-color: #4CAF50; }
  </style>
</head>
<body>
  <div class="content">
    <h2><i class="fas fa-leaf"></i> HydroSmart Control</h2>
    
    <div class="grid">
      <div class="card">
        <h3><i class="fas fa-tint"></i> Nivel Agua</h3>
        <p><span id="water_level">--</span>%</p>
      </div>
      <div class="card">
        <h3><i class="fas fa-sun"></i> Luz LDR</h3>
        <p><span id="light_val">--</span> lx</p>
      </div>
      <div class="card">
        <h3><i class="fas fa-thermometer-half"></i> Temp</h3>
        <p><span id="temp_val">--</span> &deg;C</p>
      </div>
    </div>

    <div class="card">
      <h3>Control de Bomba</h3>
      <p>Estado: <span id="pump_state" class="status-off">APAGADO</span></p>
      <button class="btn" onclick="togglePump()">INTERRUPTOR</button>
    </div>

    <div class="card">
      <h3>Rotación de Base</h3>
      <input type="range" min="0" max="180" value="90" class="slider" id="servoPos" oninput="moveBase(this.value)">
      <p>Ángulo: <span id="angle_val">90</span>&deg;</p>
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

  function onMessage(event) {
    var data = JSON.parse(event.data);
    document.getElementById('water_level').innerHTML = data.water;
    document.getElementById('light_val').innerHTML = data.light;
    document.getElementById('temp_val').innerHTML = data.temp;
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
bool pumpActive = false;
float currentTemp = 25.0;
int waterPercent = 80;

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    
    if (message == "toggle_pump") {
      pumpActive = !pumpActive;
      digitalWrite(RELE, pumpActive);
    }
    if (message.startsWith("servo:")) {
      int angle = message.substring(6).toInt();
      // Aquí mueves tu motor paso a paso o servo
    }
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
      externalHumidity = doc["main"]["humidity"];
      const char* weatherMain = doc["weather"][0]["main"];
      isRaining = (strcmp(weatherMain, "Rain") == 0);
      Serial.printf("Clima actualizado: %s, Humedad: %d%%\n", weatherMain, externalHumidity);
    }
  }
  http.end();
}

void notifyClients() {
  JsonDocument doc; 
  
  doc["water"] = waterPercent;
  doc["light"] = analogRead(PIN_LDR);
  doc["temp"] = currentTemp;
  doc["pump"] = pumpActive;
  doc["weather"] = isRaining ? "Lloviendo" : "Despejado/Nubes";
  doc["ext_hum"] = externalHumidity;
  
  // La serialización sigue funcionando igual
  String buffer;
  serializeJson(doc, buffer);
  ws.textAll(buffer);
}

void setup() {
  // ... inicialización de pines y sensores ...

  WiFi.begin(ssid, password);
  
  ws.onEvent (onEvent);
  server.addHandler(&ws);

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
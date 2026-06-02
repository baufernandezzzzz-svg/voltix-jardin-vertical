#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> 
#include <HTTPClient.h>

// --- CONFIGURACIÓN OPENWEATHERMAP ---
const String apiKey = "e41bd5ef197ab7ccdae632db3d023654";
const String city = "Buenos Aires";
const String countryCode = "AR";
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 1800000; // 30 minutos (30 * 60 * 1000)

// --- VARIABLES DE ESTADO Y SENSORES ---
bool lloviendo = false;
int humorAmbiente = 0;
float temperaturaActual = 25.0; 
int nivelAgua = 0;              
bool bombaActiva = false;
bool modoAutomatico = true;
bool luzTraseraActiva = false;

// --- ASIGNACIÓN DE PINES ---
#define BUZZER 8
#define RELE   5
#define SERVO  3
#define TRIG   2
#define ECHO   1  
#define LDR    0

// --- CONFIGURACIÓN WIFI ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// --- INSTANCIAS DEL SERVIDOR ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- INTERFAZ WEB EMBEBIDA (HTML/CSS/JS) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Voltix: Jardin Vertical</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    html { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #f9f9f9; color: #333; text-align: center; }
    body { margin: 0; padding: 10px; display: flex; flex-direction: column; align-items: center; }
    .header { width: 100%; max-width: 400px; display: flex; justify-content: space-between; align-items: center; padding: 10px 0; font-size: 1.2rem; font-weight: bold; }
    .header i { color: #666; cursor: pointer; }
    .content { width: 100%; max-width: 400px; display: flex; flex-direction: column; align-items: center; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; width: 100%; margin-top: 10px; }
    .card { background: transparent; display: flex; flex-direction: column; align-items: center; position: relative; }
    .card-title { font-size: 0.85rem; color: #777; margin-bottom: 5px; font-weight: 500; }
    .gauge-svg { width: 140px; height: 140px; transform: rotate(-220deg); }
    .gauge-bg { fill: none; stroke: #e6e6e6; stroke-width: 12; stroke-linecap: round; }
    .gauge-progress { fill: none; stroke-width: 12; stroke-linecap: round; stroke-dasharray: 330; stroke-dashoffset: 330; transition: stroke-dashoffset 0.7s ease-in-out; }
    .color-temp { stroke: #e75e4e; }
    .color-hum { stroke: #232d5a; }
    .color-soil { stroke: #c88f32; }
    .color-water { stroke: #6ecdf2; }
    .gauge-value { position: absolute; top: 45%; left: 50%; transform: translate(-50%, -50%); font-size: 1.8rem; font-weight: 300; display: flex; align-items: baseline; }
    .gauge-value span { font-size: 1rem; margin-left: 2px; color: #888; }
    .text-temp { color: #e75e4e; }
    .text-hum { color: #232d5a; }
    .text-soil { color: #c88f32; }
    .text-water { color: #6ecdf2; }
    .limits { width: 110px; display: flex; justify-content: space-between; font-size: 0.65rem; color: #bbb; margin-top: -25px; z-index: 10; }
    .status-section { display: flex; justify-content: space-around; width: 100%; margin: 25px 0 10px 0; }
    .status-block { display: flex; flex-direction: column; align-items: center; font-size: 0.8rem; color: #555; }
    .status-block span { margin-bottom: 8px; font-weight: 500; }
    .led-circle { width: 45px; height: 45px; border-radius: 50%; background-color: #d64756; box-shadow: inset -3px -3px 8px rgba(0,0,0,0.2), 0 2px 4px rgba(0,0,0,0.1); transition: background-color 0.3s; }
    .btn-pill { width: 85%; max-width: 320px; background: transparent; border: 2px solid #8e44ad; color: #8e44ad; padding: 14px; border-radius: 30px; font-size: 1.2rem; font-weight: bold; margin: 12px 0; cursor: pointer; outline: none; transition: all 0.2s ease; }
    .btn-pill:active { background-color: rgba(142, 68, 173, 0.1); }
    .btn-subtext { font-size: 0.75rem; color: #888; margin-bottom: -5px; margin-top: 10px; font-weight: 500; }
  </style>
</head>
<body>

  <div class="content">
    <div class="header">
      <i class="fas fa-arrow-left"></i>
      <span>Panel de control Voltix</span>
      <i class="fas fa-wrench"></i>
    </div>
    
    <div class="grid">
      <div class="card">
        <div class="card-title">Temperatura</div>
        <svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-temp" id="g_temp" cx="70" cy="70" r="53"></circle></svg>
        <div class="gauge-value text-temp"><div id="v_temp">--</div><span>°C</span></div>
        <div class="limits"><span>0</span><span>100</span></div>
      </div>

      <div class="card">
        <div class="card-title">Humedad</div>
        <svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-hum" id="g_hum" cx="70" cy="70" r="53"></circle></svg>
        <div class="gauge-value text-hum"><div id="v_hum">--</div><span>%</span></div>
        <div class="limits"><span>0</span><span>100</span></div>
      </div>

      <div class="card">
        <div class="card-title">Intensidad de luz</div>
        <svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-soil" id="g_soil" cx="70" cy="70" r="53"></circle></svg>
        <div class="gauge-value text-soil"><div id="v_soil">--</div><span>%</span></div>
        <div class="limits"><span>0</span><span>100</span></div>
      </div>

      <div class="card">
        <div class="card-title">Nivel de agua</div>
        <svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-water" id="g_water" cx="70" cy="70" r="53"></circle></svg>
        <div class="gauge-value text-water"><div id="v_water">--</div><span>%</span></div>
        <div class="limits"><span>0</span><span>100</span></div>
      </div>
    </div>

    <div class="status-section">
      <div class="status-block">
        <span>Estado</span>
        <div class="led-circle" id="status_led"></div>
      </div>
      <div class="status-block">
        <span>Luz trasera</span>
        <button class="btn-pill" style="width:75px; padding:6px; font-size:0.9rem; margin:0;" id="btn_strip" onclick="toggleStrip()">OFF</button>
      </div>
    </div>
    
    <div class="btn-subtext">Bomba</div>
    <button class="btn-pill" id="btn_pump" onclick="togglePump()">Apagada</button>
    
    <div class="btn-subtext">Rotación</div>
    <button class="btn-pill" id="btn_mode" onclick="toggleMode()">Automático</button>
  </div>

<script>
 var gateway = `ws://${window.location.host}/ws`;
  var websocket;
  var isAutoMode = true;
  window.addEventListener('load', initWebSocket);

  function initWebSocket() {
    websocket = new WebSocket(gateway);
    websocket.onmessage = onMessage;
    websocket.onclose = function() { setTimeout(initWebSocket, 2000); };
  }

  function updateGauge(gaugeId, textId, val) {
    if(val > 100) val = 100;
    if(val < 0) val = 0;
    var circle = document.getElementById(gaugeId);
    var offset = 330 - (val / 100) * 250; 
    circle.style.strokeDashoffset = offset;
    document.getElementById(textId).innerText = Math.round(val);
  }

  function onMessage(event) {
    var data = JSON.parse(event.data);
    
    updateGauge('g_temp', 'v_temp', data.temp);
    updateGauge('g_hum', 'v_hum', data.ext_hum);
    
    // Conversión del LDR (0-4095) a Porcentaje en el cliente
    var ldrPercent = (data.light / 4095) * 100;
    updateGauge('g_soil', 'v_soil', ldrPercent);
    
    updateGauge('g_water', 'v_water', data.water);

    // Actualización del botón de la Bomba y LED de Estado
    var pumpBtn = document.getElementById('btn_pump');
    if(data.pump) {
      pumpBtn.innerText = "Pump On";
      pumpBtn.style.background = "#8e44ad";
      pumpBtn.style.color = "#fff";
      document.getElementById('status_led').style.backgroundColor = "#2ecc71"; 
    } else {
      pumpBtn.innerText = "Pump Off";
      pumpBtn.style.background = "transparent";
      pumpBtn.style.color = "#8e44ad";
      document.getElementById('status_led').style.backgroundColor = "#d64756"; 
    }
  }

  function togglePump() { websocket.send('toggle_pump'); }
  
  function toggleMode() {
    isAutoMode = !isAutoMode;
    var modeBtn = document.getElementById('btn_mode');
    modeBtn.innerText = isAutoMode ? "Automatic" : "Manual";
    websocket.send('mode:' + (isAutoMode ? 'auto' : 'manual'));
  }

  function toggleStrip() {
    var stripBtn = document.getElementById('btn_strip');
    if(stripBtn.innerText === "OFF") {
      stripBtn.innerText = "ON";
      stripBtn.style.background = "#8e44ad";
      stripBtn.style.color = "#fff";
      websocket.send('strip:on');
    } else {
      stripBtn.innerText = "OFF";
      stripBtn.style.background = "transparent";
      stripBtn.style.color = "#8e44ad";
      websocket.send('strip:off');
    }
  }
</script>
</body>
</html>
)rawliteral";

// --- ENVÍO DE DATOS JSON POR WEBSOCKET ---
void notifyClients() {
  JsonDocument doc; 
  
  doc["water"] = nivelAgua;
  doc["light"] = analogRead(LDR); // Envía valor crudo (0-4095)
  doc["temp"] = temperaturaActual;
  doc["pump"] = bombaActiva;
  doc["weather"] = lloviendo ? "Lloviendo" : "Despejado/Nubes";
  doc["ext_hum"] = humorAmbiente;
  
  String buffer;
  serializeJson(doc, buffer);
  ws.textAll(buffer);
}

// --- MEDICIÓN DEL SENSOR ULTRASÓNICO ---
int obtenerPorcentajeAgua() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  
  long duracion = pulseIn(ECHO, HIGH, 26000); 
  if (duracion == 0) return 0;

  float distancia = duracion * 0.034 / 2;
  
  // Mapeo: 30cm = 0% (Vacío), 4cm = 100% (Lleno)
  int porcentaje = map(distancia, 30, 4, 0, 100);
  return constrain(porcentaje, 0, 100);
}

// --- PROCESAMIENTO DE ACCIONES DESDE LA WEB ---
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    
    // Control manual de la bomba
    if (message == "toggle_pump") {
      bombaActiva = !bombaActiva;
      digitalWrite(RELE, bombaActiva ? HIGH : LOW);
    }
    // Control de modos (Automático / Manual)
    else if (message == "mode:auto") {
      modoAutomatico = true;
    } 
    else if (message == "mode:manual") {
      modoAutomatico = false;
    }
    // Control de luz trasera (Luz de cortesía / Tira led de ejemplo)
    else if (message == "strip:on") {
      luzTraseraActiva = true;
      // Puedes asignar un pin físico aquí si lo deseas, ej: digitalWrite(PIN_LED, HIGH);
    }
    else if (message == "strip:off") {
      luzTraseraActiva = false;
      // ej: digitalWrite(PIN_LED, LOW);
    }

    // Al cambiar cualquier estado interno, respondemos de inmediato a la web
    notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Cliente WebSocket conectado desde: %s\n", client->remoteIP().toString().c_str());
      notifyClients(); // Enviar estado actual al conectar un nuevo dispositivo
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Cliente WebSocket desconectado\n");
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// --- CONSULTA API OPENWEATHERMAP ---
void updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";
    
    http.begin(url);
    int httpCode = http.GET();
  
    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        humorAmbiente = doc["main"]["humidity"];
        temperaturaActual = doc["main"]["temp"]; 
        const char* weatherMain = doc["weather"][0]["main"];
        lloviendo = (strcmp(weatherMain, "Rain") == 0);
        Serial.printf("Clima actualizado: %s, Temp: %.2f C, Humedad: %d%%\n", weatherMain, temperaturaActual, humorAmbiente);
      }
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);

  // Inicialización de Pines de hardware
  pinMode(RELE, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  
  digitalWrite(RELE, LOW); // La bomba arranca apagada

  // Conexión a la red WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConexión WiFi Establecida!");
  Serial.print("Dirección IP local: ");
  Serial.println(WiFi.localIP());
  
  // Configurar WebSocket y Servidor HTTP
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.begin();

  // Primer llamado meteorológico
  updateWeather();
  lastWeatherUpdate = millis();
}

void loop() {
  ws.cleanupClients();
  
  // Consulta asíncrona a OpenWeatherMap cada 30 minutos
  if (millis() - lastWeatherUpdate > weatherInterval) {
    updateWeather();
    lastWeatherUpdate = millis();
  }

  // Lectura física de sensores locales y actualización web cada 2 segundos
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    nivelAgua = obtenerPorcentajeAgua(); 
    
    notifyClients(); 
    lastUpdate = millis();
  }
}
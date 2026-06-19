#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> 
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <ESP32Servo.h> // NUEVO: Librería para controlar el servo
#include <time.h>       // NUEVO: Librería para manejar la hora real

// --- INSTANCIA DEL SENSOR BMP180 ---
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// --- CONFIGURACIÓN OPENWEATHERMAP ---
const String apiKey = "e41bd5ef197ab7ccdae632db3d023654";
const String city = "Buenos Aires";
const String countryCode = "AR";
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 1800000; // 30 minutos

// --- NUEVO: VARIABLES DE TIEMPO Y SOL ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800; // GMT-3 (Buenos Aires)
const int   daylightOffset_sec = 0;
unsigned long sunrise = 0; // Hora de amanecer (Unix Timestamp)
unsigned long sunset = 0;  // Hora de atardecer (Unix Timestamp)

// --- VARIABLES DE ESTADO Y SENSORES ---
bool lloviendo = false;
int humorAmbiente = 0;
float temperaturaActual = 0.0;
int nivelAgua = 0;              
bool bombaActiva = false;
bool modoAutomatico = true;
bool luzTraseraActiva = false;

// --- ASIGNACIÓN DE PINES ---
#define BUZZER 8
#define RELE   5
#define SERVO  3  // Pin del servo (Asegúrate de que no interfiera con TX/RX)
#define TRIG   2
#define ECHO   1  
#define LDR    0
#define BMP_SDA 7
#define BMP_SCL 6
#define PIN_LUZ_TRASERA 4 

// --- INSTANCIAS ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Servo baseServo; // NUEVO: Instancia del servo

// --- CONFIGURACIÓN WIFI ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// --- INTERFAZ WEB EMBEBIDA (Se mantiene igual) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Voltix: Jardin Vertical</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    /* ... (El CSS original se mantiene intacto para ahorrar espacio) ... */
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
    <div class="header"><i class="fas fa-arrow-left"></i><span>Panel de control Voltix</span><i class="fas fa-wrench"></i></div>
    <div class="grid">
      <div class="card"><div class="card-title">Temperatura</div><svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-temp" id="g_temp" cx="70" cy="70" r="53"></circle></svg><div class="gauge-value text-temp"><div id="v_temp">--</div><span>°C</span></div><div class="limits"><span>0</span><span>100</span></div></div>
      <div class="card"><div class="card-title">Humedad</div><svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-hum" id="g_hum" cx="70" cy="70" r="53"></circle></svg><div class="gauge-value text-hum"><div id="v_hum">--</div><span>%</span></div><div class="limits"><span>0</span><span>100</span></div></div>
      <div class="card"><div class="card-title">Intensidad de luz</div><svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-soil" id="g_soil" cx="70" cy="70" r="53"></circle></svg><div class="gauge-value text-soil"><div id="v_soil">--</div><span>%</span></div><div class="limits"><span>0</span><span>100</span></div></div>
      <div class="card"><div class="card-title">Nivel de agua</div><svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-water" id="g_water" cx="70" cy="70" r="53"></circle></svg><div class="gauge-value text-water"><div id="v_water">--</div><span>%</span></div><div class="limits"><span>0</span><span>100</span></div></div>
    </div>
    <div class="status-section">
      <div class="status-block"><span>Estado</span><div class="led-circle" id="status_led"></div></div>
      <div class="status-block"><span>Luz trasera</span><button class="btn-pill" style="width:75px; padding:6px; font-size:0.9rem; margin:0;" id="btn_strip" onclick="toggleStrip()">OFF</button></div>
    </div>
    <div class="btn-subtext">Bomba</div><button class="btn-pill" id="btn_pump" onclick="togglePump()">Apagada</button>
    <div class="btn-subtext">Rotación</div><button class="btn-pill" id="btn_mode" onclick="toggleMode()">Automático</button>
  </div>

<script>
 var gateway = `ws://${window.location.host}/ws`;
  var websocket;
  var isAutoMode = true;
  window.addEventListener('load', initWebSocket);

  function initWebSocket() { websocket = new WebSocket(gateway); websocket.onmessage = onMessage; websocket.onclose = function() { setTimeout(initWebSocket, 2000); }; }
  function updateGauge(gaugeId, textId, val) { if(val > 100) val = 100; if(val < 0) val = 0; var circle = document.getElementById(gaugeId); circle.style.strokeDashoffset = 330 - (val / 100) * 250; document.getElementById(textId).innerText = Math.round(val); }

  function onMessage(event) {
    var data = JSON.parse(event.data);
    updateGauge('g_temp', 'v_temp', data.temp);
    updateGauge('g_hum', 'v_hum', data.ext_hum);
    updateGauge('g_soil', 'v_soil', (data.light / 4095) * 100);
    updateGauge('g_water', 'v_water', data.water);

    var pumpBtn = document.getElementById('btn_pump');
    if(data.pump) { pumpBtn.innerText = "Pump On"; pumpBtn.style.background = "#8e44ad"; pumpBtn.style.color = "#fff"; document.getElementById('status_led').style.backgroundColor = "#2ecc71"; } 
    else { pumpBtn.innerText = "Pump Off"; pumpBtn.style.background = "transparent"; pumpBtn.style.color = "#8e44ad"; document.getElementById('status_led').style.backgroundColor = "#d64756"; }
  }

  function togglePump() { websocket.send('toggle_pump'); }
  function toggleMode() { isAutoMode = !isAutoMode; document.getElementById('btn_mode').innerText = isAutoMode ? "Automatic" : "Manual"; websocket.send('mode:' + (isAutoMode ? 'auto' : 'manual')); }
  function toggleStrip() {
    var stripBtn = document.getElementById('btn_strip');
    if(stripBtn.innerText === "OFF") { stripBtn.innerText = "ON"; stripBtn.style.background = "#8e44ad"; stripBtn.style.color = "#fff"; websocket.send('strip:on'); } 
    else { stripBtn.innerText = "OFF"; stripBtn.style.background = "transparent"; stripBtn.style.color = "#8e44ad"; websocket.send('strip:off'); }
  }
</script>
</body>
</html>
)rawliteral";

// --- ENVÍO DE DATOS JSON POR WEBSOCKET ---
void notifyClients() {
  JsonDocument doc; 
  doc["water"] = nivelAgua;
  doc["light"] = analogRead(LDR); 
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
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duracion = pulseIn(ECHO, HIGH, 26000); 
  if (duracion == 0) return 0;
  float distancia = duracion * 0.034 / 2;
  return constrain(map(distancia, 30, 4, 0, 100), 0, 100);
}

// --- PROCESAMIENTO DE ACCIONES WEB ---
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; String message = (char*)data;
    if (message == "toggle_pump") { bombaActiva = !bombaActiva; digitalWrite(RELE, bombaActiva ? HIGH : LOW); }
    else if (message == "mode:auto") { modoAutomatico = true; } 
    else if (message == "mode:manual") { modoAutomatico = false; baseServo.write(90); } // Detener servo en manual
    else if (message == "strip:on") { luzTraseraActiva = true; digitalWrite(PIN_LUZ_TRASERA, HIGH); }
    else if (message == "strip:off") { luzTraseraActiva = false; digitalWrite(PIN_LUZ_TRASERA, LOW); }
    notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) handleWebSocketMessage(arg, data, len);
}

// --- CONSULTA API OPENWEATHERMAP ---
void updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";
    http.begin(url);
    if (http.GET() > 0) {
      JsonDocument doc;
      if (!deserializeJson(doc, http.getString())) {
        humorAmbiente = doc["main"]["humidity"];
        lloviendo = (strcmp(doc["weather"][0]["main"], "Rain") == 0);
        
        // MODIFICACIÓN: Capturar horarios del sol
        sunrise = doc["sys"]["sunrise"];
        sunset = doc["sys"]["sunset"];
        
        Serial.printf("Clima actualizado. Amanecer: %lu, Atardecer: %lu\n", sunrise, sunset);
      }
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELE, OUTPUT); pinMode(BUZZER, OUTPUT);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  pinMode(PIN_LUZ_TRASERA, OUTPUT); digitalWrite(PIN_LUZ_TRASERA, LOW);
  digitalWrite(RELE, LOW); 

  // Iniciar Servo
  ESP32PWM::allocateTimer(0);
  baseServo.setPeriodHertz(50);
  baseServo.attach(SERVO, 500, 2400); 
  baseServo.write(90); // 90 grados = Detenido en servos de rotación continua

  Wire.begin(BMP_SDA, BMP_SCL);
  bmp.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Conectado!");

  // NUEVO: Sincronizar hora del reloj interno vía Internet (NTP)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
  server.begin();

  updateWeather();
  lastWeatherUpdate = millis();
}

void loop() {
  ws.cleanupClients();
  
  if (millis() - lastWeatherUpdate > weatherInterval) {
    updateWeather();
    lastWeatherUpdate = millis();
  }

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    nivelAgua = obtenerPorcentajeAgua(); 
    bmp.getTemperature(&temperaturaActual);
    int lecturaLDR = analogRead(LDR);
    
    // --- LÓGICA AUTOMÁTICA (Bomba + Servo) ---
    if (modoAutomatico) {
       
       // 1. Lógica de la Bomba de Agua
       if (!lloviendo && temperaturaActual > 28 && lecturaLDR > 2500) {
           digitalWrite(RELE, HIGH); bombaActiva = true;
           notifyClients(); 
           delay(2200);     
           digitalWrite(RELE, LOW);  bombaActiva = false;
       }

       // 2. NUEVO ALGORITMO: Lógica de Rotación Solar
       time_t now; 
       time(&now); // Obtiene la hora actual UNIX

       // Verifica si estamos en horario de luz solar según OpenWeather
       if (now >= sunrise && now <= sunset) {
           
           // Evaluamos la luz real in situ. 
           // Suponemos que LDR > 1000 es luz útil para las plantas.
           if (lecturaLDR > 1000) {
               
               // MAPEO INTELIGENTE: 
               // Mientras MÁS luz detecta el LDR (1000 a 4095),
               // MÁS rápido gira el motor (92 a 180).
               // (90 es motor apagado, 92 es giro muy lento, 180 es máximo).
               int velocidadGiro = map(lecturaLDR, 1000, 4095, 92, 180);
               
               // Para que no se vuelva loco el servo por fluctuaciones, lo limitamos.
               velocidadGiro = constrain(velocidadGiro, 90, 180);
               
               baseServo.write(velocidadGiro);
           } else {
               // Si está nublado o bajo sombra muy pesada, se detiene para ahorrar energía.
               baseServo.write(90); 
           }
       } else {
           // Es de noche, detenemos la base.
           baseServo.write(90); 
       }
    }
    
    notifyClients(); 
    lastUpdate = millis();
  }
}
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> 
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <ESP32Servo.h> 
#include <time.h>        
#include <Adafruit_NeoPixel.h> 

// --- INSTANCIA DEL SENSOR BMP180 ---
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// --- CONFIGURACIÓN OPENWEATHERMAP ---
const String apiKey = "e41bd5ef197ab7ccdae632db3d023654";
const String city = "Buenos Aires";
const String countryCode = "AR";
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 1800000; // 30 minutos

// --- VARIABLES DE TIEMPO Y SOL ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800; // GMT-3 (Buenos Aires)
const int   daylightOffset_sec = 0;
unsigned long sunrise = 0; 
unsigned long sunset = 0;  

// --- VARIABLES DE ESTADO Y SENSORES ---
bool lloviendo = false;
int humorAmbiente = 0;
float temperaturaActual = 0.0;
float nivelAgua = 0.0; // Mantiene el valor en porcentaje (0 a 100%)
bool bombaActiva = false;
bool modoAutomatico = true;
bool luzTraseraActiva = false;
int velocidadGiroActual = 90; 

// --- ASIGNACIÓN DE PINES ---
#define BUZZER 8
#define RELE   5
#define SERVO  3  
#define TRIG   2
#define ECHO   1  
#define LDR    0
#define BMP_SDA 7
#define BMP_SCL 6
#define PIN_ANILLO_LED 10 

// --- CONFIGURACIÓN DEL ANILLO LED ---
#define NUM_LEDS 16 
Adafruit_NeoPixel anillo = Adafruit_NeoPixel(NUM_LEDS, PIN_ANILLO_LED, NEO_GRB + NEO_KHZ800);

// --- INSTANCIAS ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Servo baseServo; 

// --- CONFIGURACIÓN WIFI ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// --- INTERFAZ WEB EMBEBIDA ---
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
    <div class="header"><i class="fas fa-arrow-left"></i><span>Panel de control Voltix</span><i class="fas fa-wrench"></i></div>
    <div class="grid">
      <div class="card"><div class="card-title">Temperatura</div><svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-temp" id="g_temp" cx="70" cy="70" r="53"></circle></svg><div class="gauge-value text-temp"><div id="v_temp">--</div><span>°C</span></div><div class="limits"><span>0</span><span>100</span></div></div>
      <div class="card"><div class="card-title">Humedad</div><svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-hum" id="g_hum" cx="70" cy="70" r="53"></circle></svg><div class="gauge-value text-hum"><div id="v_hum">--</div><span>%</span></div><div class="limits"><span>0</span><span>100</span></div></div>
      <div class="card"><div class="card-title">Intensidad de luz</div><svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-soil" id="g_soil" cx="70" cy="70" r="53"></circle></svg><div class="gauge-value text-soil"><div id="v_soil">--</div><span>%</span></div><div class="limits"><span>0</span><span>100</span></div></div>
      <div class="card"><div class="card-title">Nivel de agua</div><svg class="gauge-svg"><circle class="gauge-bg" cx="70" cy="70" r="53"></circle><circle class="gauge-progress color-water" id="g_water" cx="70" cy="70" r="53"></circle></svg><div class="gauge-value text-water"><div id="v_water">--</div><span>%</span></div><div class="limits"><span>0</span><span>100</span></div></div>
    </div>
    <div class="status-section">
      <div class="status-block"><span>Estado</span><div class="led-circle" id="status_led"></div></div>
      <div class="status-block"><span>Anillo LED</span><button class="btn-pill" style="width:75px; padding:6px; font-size:0.9rem; margin:0;" id="btn_strip" onclick="toggleStrip()">OFF</button></div>
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
  function updateGauge(gaugeId, textId, val, maxVal = 100) { 
    if(val > maxVal) val = maxVal; if(val < 0) val = 0; 
    var circle = document.getElementById(gaugeId); 
    circle.style.strokeDashoffset = 330 - (val / maxVal) * 250; 
    document.getElementById(textId).innerText = Math.round(val); 
  }
  function onMessage(event) {
    var data = JSON.parse(event.data);
    updateGauge('g_temp', 'v_temp', data.temp, 100);
    updateGauge('g_hum', 'v_hum', data.ext_hum, 100);
    updateGauge('g_soil', 'v_soil', (data.light / 4095) * 100, 100);
    updateGauge('g_water', 'v_water', data.water, 100);
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

// --- MEDICIÓN MODIFICADA (LÍMITE ESTRICTO DE 30 CM) ---
float obtenerNivelAguaPorcentaje() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  
  long duracion = pulseIn(ECHO, HIGH); 
  
  if (duracion == 0) return 0.0;
  
  float distancia = duracion * 0.2834;
  Serial.print ("[DEBUG] ");
  Serial.println (distancia);
  
  // Si la distancia es mayor a 30 cm, la ignoramos y devolvemos 0% (vacío)
  if (distancia > 30.0) {
    return 0.0;
  }
  
  // Regla solicitada: 0 cm = 100%, 30 cm = 0%
  float porcentaje = ((30.0 - distancia) / 28.0) * 100.0;
  
  // Limitamos los valores inferiores a 0 y superiores a 100 por seguridad extra
  if(porcentaje < 0.0) porcentaje = 0.0;
  if(porcentaje > 100.0) porcentaje = 100.0;
  
  return porcentaje;
}

// --- PROCESAMIENTO DE ACCIONES WEB ---
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; String message = (char*)data;
    if (message == "toggle_pump") { bombaActiva = !bombaActiva; digitalWrite(RELE, bombaActiva ? HIGH : LOW); }
    else if (message == "mode:auto") { modoAutomatico = true; } 
    else if (message == "mode:manual") { modoAutomatico = false; velocidadGiroActual = 90; baseServo.write(90); } 
    else if (message == "strip:on") { luzTraseraActiva = true; } 
    else if (message == "strip:off") { luzTraseraActiva = false; }
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
        sunrise = doc["sys"]["sunrise"];
        sunset = doc["sys"]["sunset"];
      }
    }
    http.end();
  }
}

// --- CONTROL DE EFECTOS DEL ANILLO LED ---
void actualizarAnilloLED() {
  static unsigned long lastLEDMillis = 0;
  static int estadoEfectoActual = -1;
  int nuevoEstado = 0;

  if (nivelAgua < 33.0) { nuevoEstado = 1; } 
  else if (bombaActiva) { nuevoEstado = 2; } 
  else if (modoAutomatico && velocidadGiroActual > 90) { nuevoEstado = 3; } 
  else if (WiFi.status() != WL_CONNECTED) { nuevoEstado = 4; }

  if (nuevoEstado != estadoEfectoActual) { estadoEfectoActual = nuevoEstado; anillo.clear(); }
  unsigned long currentMillis = millis();

  switch (estadoEfectoActual) {
    case 1: { 
      static bool ledState = false;
      if (currentMillis - lastLEDMillis > 100) { lastLEDMillis = currentMillis; ledState = !ledState;
        for (int i = 0; i < NUM_LEDS; i++) anillo.setPixelColor(i, ledState ? anillo.Color(255, 0, 0) : anillo.Color(0, 0, 0));
        anillo.show(); } break;
    }
    case 2: { 
      static int desplazar = 0;
      if (currentMillis - lastLEDMillis > 60) { lastLEDMillis = currentMillis;
        for (int i = 0; i < NUM_LEDS; i++) { int brillo = (sin((i + desplazar) * 0.6) + 1) * 100 + 50; anillo.setPixelColor(i, anillo.Color(0, brillo / 2, brillo)); }
        anillo.show(); desplazar++; } break;
    }
    case 3: { 
      static int posMarquesina = 0;
      int tiempoGiroLED = constrain(map(velocidadGiroActual, 90, 180, 150, 30), 30, 150);
      if (currentMillis - lastLEDMillis > tiempoGiroLED) { lastLEDMillis = currentMillis; anillo.clear();
        anillo.setPixelColor(posMarquesina, anillo.Color(0, 255, 100)); anillo.setPixelColor((posMarquesina + 1) % NUM_LEDS, anillo.Color(0, 180, 70)); anillo.setPixelColor((posMarquesina + 2) % NUM_LEDS, anillo.Color(0, 100, 40));
        anillo.show(); posMarquesina = (posMarquesina + 1) % NUM_LEDS; } break;
    }
    case 4: { 
      static int brilloRespiracion = 0; static int direccionFade = 4;
      if (currentMillis - lastLEDMillis > 20) { lastLEDMillis = currentMillis; brilloRespiracion += direccionFade;
        if (brilloRespiracion <= 5 || brilloRespiracion >= 250) direccionFade = -direccionFade; 
        brilloRespiracion = constrain(brilloRespiracion, 5, 250);
        for (int i = 0; i < NUM_LEDS; i++) anillo.setPixelColor(i, anillo.Color(0, 0, brilloRespiracion));
        anillo.show(); } break;
    }
    default: { if (luzTraseraActiva) for (int i = 0; i < NUM_LEDS; i++) anillo.setPixelColor(i, anillo.Color(100, 130, 100)); else anillo.clear(); anillo.show(); break; }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELE, OUTPUT); pinMode(BUZZER, OUTPUT);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  digitalWrite(RELE, LOW); 
  anillo.begin(); anillo.show(); 
  ESP32PWM::allocateTimer(0);
  baseServo.setPeriodHertz(50);
  baseServo.attach(SERVO, 500, 2400); 
  baseServo.write(90); 
  Wire.begin(BMP_SDA, BMP_SCL); bmp.begin();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
  server.begin();
  updateWeather(); lastWeatherUpdate = millis();
}

void loop() {
  ws.cleanupClients();
  actualizarAnilloLED(); 
  if (millis() - lastWeatherUpdate > weatherInterval) { updateWeather(); lastWeatherUpdate = millis(); }
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    nivelAgua = obtenerNivelAguaPorcentaje(); 
    bmp.getTemperature(&temperaturaActual);
    int lecturaLDR = analogRead(LDR);
    if (modoAutomatico) {
       if (!lloviendo && temperaturaActual > 28 && lecturaLDR > 2500) {
           if (nivelAgua > 33.0) {
               digitalWrite(RELE, HIGH); bombaActiva = true; notifyClients(); delay(2200); digitalWrite(RELE, LOW); bombaActiva = false;
           }
       }
       time_t now; time(&now); 
       if (now >= sunrise && now <= sunset) {
           if (lecturaLDR > 1000) {
               velocidadGiroActual = constrain(map(lecturaLDR, 1000, 4095, 92, 180), 90, 180);
               baseServo.write(velocidadGiroActual);
           } else { velocidadGiroActual = 90; baseServo.write(velocidadGiroActual); }
       } else { velocidadGiroActual = 90; baseServo.write(velocidadGiroActual); }
    }
    notifyClients(); lastUpdate = millis();
  }
}
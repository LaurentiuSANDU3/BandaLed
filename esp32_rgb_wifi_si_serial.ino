#include <WiFi.h>
#include <WebServer.h>

// ----------- CONFIGURARE WIFI -----------
const char* WIFI_SSID     = ".";
const char* WIFI_PASSWORD = "12345678";

// ----------- PINI LED RGBWY -----------
// ATENTIE: mutati fizic LED-urile pe acesti pini!

// cand aveti nevoie si de Serial pentru comenzile vocale.
const int PIN_RED    = 13;
const int PIN_GREEN  = 14;
const int PIN_BLUE   = 27;
const int PIN_WHITE  = 26;
const int PIN_YELLOW = 25;

const int PWM_FREQ       = 5000;
const int PWM_RESOLUTION = 8;

struct LedState {
  int r = 0;
  int g = 0;
  int b = 0;
  int w = 0;
  int y = 0;
  int brightness = 100; // procent global
} state;

WebServer server(80);

// ----------- APLICARE PE HARDWARE -----------
void applyOutput() {
  float scale = state.brightness / 100.0f;
  ledcWrite(PIN_RED,    (int)(state.r * scale));
  ledcWrite(PIN_GREEN,  (int)(state.g * scale));
  ledcWrite(PIN_BLUE,   (int)(state.b * scale));
  ledcWrite(PIN_WHITE,  (int)(state.w * scale));
  ledcWrite(PIN_YELLOW, (int)(state.y * scale));
}

int clampByte(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return v;
}

void setChannels(int r, int g, int b, int w, int y) {
  state.r = r; state.g = g; state.b = b; state.w = w; state.y = y;
}

//  MAPARE MODURI, folosita si de Serial si de web
// mod1 = rosu, mod2 = galben , mod3 = verde,
// mod4 = albastru, mod5 = alb 
void applyMode(int mod) {
  switch (mod) {
    case 1: setChannels(255, 0,   0,   0,   0);   break; // rosu
    case 2: setChannels(0,   0,   0,   0,   255); break; // galben (Y)
    case 3: setChannels(0,   255, 0,   0,   0);   break; // verde
    case 4: setChannels(0,   0,   255, 0,   0);   break; // albastru
    case 5: setChannels(0,   0,   0,   255, 0);   break; // alb (W)
    default:
      Serial.println("Mod necunoscut: " + String(mod));
      return;
  }
  if (state.brightness == 0) state.brightness = 100; // daca era stins, il aprindem
  applyOutput();
  Serial.println("Mod aplicat: " + String(mod));
}

// ----------- HANDLERE HTTP -----------

// GET /set?r=255&g=0&b=0&w=0&y=0  (orice subset de parametri e acceptat)
void handleSet() {
  if (server.hasArg("r")) state.r = clampByte(server.arg("r").toInt());
  if (server.hasArg("g")) state.g = clampByte(server.arg("g").toInt());
  if (server.hasArg("b")) state.b = clampByte(server.arg("b").toInt());
  if (server.hasArg("w")) state.w = clampByte(server.arg("w").toInt());
  if (server.hasArg("y")) state.y = clampByte(server.arg("y").toInt());
  applyOutput();
  server.send(200, "text/plain", "OK");
}

// GET /mode?m=1..5   (acelasi efect ca o comanda vocala "mod X")
void handleMode() {
  if (server.hasArg("m")) {
    applyMode(server.arg("m").toInt());
  }
  server.send(200, "text/plain", "OK");
}

// GET /brightness?value=0-100
void handleBrightness() {
  if (server.hasArg("value")) {
    int v = server.arg("value").toInt();
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    state.brightness = v;
    applyOutput();
  }
  server.send(200, "text/plain", "OK");
}

void handleOff() {
  state.brightness = 0;
  applyOutput();
  server.send(200, "text/plain", "OK");
}

void handleOn() {
  if (state.brightness == 0) state.brightness = 100;
  applyOutput();
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String out = "{";
  out += "\"r\":" + String(state.r) + ",";
  out += "\"g\":" + String(state.g) + ",";
  out += "\"b\":" + String(state.b) + ",";
  out += "\"w\":" + String(state.w) + ",";
  out += "\"y\":" + String(state.y) + ",";
  out += "\"brightness\":" + String(state.brightness);
  out += "}";
  server.send(200, "application/json", out);
}

// ----------- PAGINA WEB -----------
const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ro">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Control LED RGBWY - ESP32</title>
<style>
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100vh; background: #12141a;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    color: #eee; display: flex; justify-content: center; padding: 24px 16px 60px;
  }
  .card {
    width: 100%; max-width: 420px; background: #1c1f27;
    border-radius: 20px; padding: 24px; box-shadow: 0 10px 30px rgba(0,0,0,0.4);
  }
  h1 { font-size: 20px; text-align: center; margin: 0 0 4px; font-weight: 600; }
  .subtitle { text-align: center; font-size: 13px; color: #888; margin-bottom: 20px; }
  .modes {
    display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; margin-bottom: 20px;
  }
  .mode-btn {
    aspect-ratio: 1; border-radius: 16px; border: 3px solid transparent;
    display: flex; align-items: center; justify-content: center;
    font-weight: 600; font-size: 13px; cursor: pointer; user-select: none;
    transition: transform 0.08s ease; color: #fff; text-shadow: 0 1px 3px rgba(0,0,0,0.6);
  }
  .mode-btn:active { transform: scale(0.94); }
  #m1 { background: #7a0f0f; } #m2 { background: #8a7000; color:#222; text-shadow:none; }
  #m3 { background: #0f5c1f; } #m4 { background: #12308f; }
  #m5 { background: #3a3a3a; }
  .power-row { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 18px; }
  .power-btn { padding: 14px 0; border-radius: 14px; border: none; font-size: 15px; font-weight: 700; cursor: pointer; }
  #btn-on  { background: #1eb83c; color: #fff; }
  #btn-off { background: #b8281e; color: #fff; }
  .slider-label { display: flex; justify-content: space-between; font-size: 13px; color: #aaa; margin-bottom: 8px; }
  .slider-block { margin-top: 16px; }
  input[type=range] { width: 100%; }
  .picker-row { margin-top: 20px; text-align: center; }
  input[type=color] { width: 60px; height: 44px; border: none; border-radius: 10px; background: none; }
  #status { text-align: center; font-size: 12px; color: #555; margin-top: 18px; min-height: 14px; }
</style>
</head>
<body>
  <div class="card">
    <h1>Control LED RGBWY</h1>
    <div class="subtitle">Moduri rapide (identice cu comenzile vocale)</div>

    <div class="modes">
      <div class="mode-btn" id="m1" data-mod="1">Mod 1<br>Roșu</div>
      <div class="mode-btn" id="m2" data-mod="2">Mod 2<br>Galben (Y)</div>
      <div class="mode-btn" id="m3" data-mod="3">Mod 3<br>Verde</div>
      <div class="mode-btn" id="m4" data-mod="4">Mod 4<br>Albastru</div>
      <div class="mode-btn" id="m5" data-mod="5">Mod 5<br>Alb (W)</div>
    </div>

    <div class="power-row">
      <button class="power-btn" id="btn-on">ON</button>
      <button class="power-btn" id="btn-off">OFF</button>
    </div>

    <div class="slider-block">
      <div class="slider-label"><span>Luminozitate</span><span id="brightness-value">100%</span></div>
      <input type="range" id="brightness" min="0" max="100" value="100">
    </div>

    <div class="slider-block">
      <div class="slider-label"><span>Canal Alb (W)</span><span id="white-value">0</span></div>
      <input type="range" id="whitechan" min="0" max="255" value="0">
    </div>

    <div class="slider-block">
      <div class="slider-label"><span>Canal Galben (Y)</span><span id="yellow-value">0</span></div>
      <input type="range" id="yellowchan" min="0" max="255" value="0">
    </div>

    <div class="picker-row">
      <div style="font-size:13px;color:#aaa;margin-bottom:8px;">Culoare custom (R/G/B)</div>
      <input type="color" id="colorpicker" value="#ff0000">
    </div>

    <div id="status"></div>
  </div>

<script>
  function el(id){ return document.getElementById(id); }
  function setStatusMsg(msg){
    el('status').textContent = msg;
    if (msg) setTimeout(() => { el('status').textContent = ''; }, 1500);
  }

  document.querySelectorAll('.mode-btn').forEach(btn => {
    btn.addEventListener('click', async () => {
      const mod = btn.dataset.mod;
      try {
        await fetch(`/mode?m=${mod}`);
        setStatusMsg('Mod ' + mod + ' aplicat');
      } catch(e) { setStatusMsg('Eroare conexiune'); }
    });
  });

  el('btn-on').addEventListener('click', async () => {
    try { await fetch('/on'); setStatusMsg('Aprins'); } catch(e){ setStatusMsg('Eroare conexiune'); }
  });
  el('btn-off').addEventListener('click', async () => {
    try { await fetch('/off'); setStatusMsg('Stins'); } catch(e){ setStatusMsg('Eroare conexiune'); }
  });

  let brTimeout;
  el('brightness').addEventListener('input', (e) => {
    el('brightness-value').textContent = e.target.value + '%';
    clearTimeout(brTimeout);
    brTimeout = setTimeout(() => fetch(`/brightness?value=${e.target.value}`), 120);
  });

  let wTimeout;
  el('whitechan').addEventListener('input', (e) => {
    el('white-value').textContent = e.target.value;
    clearTimeout(wTimeout);
    wTimeout = setTimeout(() => fetch(`/set?w=${e.target.value}`), 120);
  });

  let yTimeout;
  el('yellowchan').addEventListener('input', (e) => {
    el('yellow-value').textContent = e.target.value;
    clearTimeout(yTimeout);
    yTimeout = setTimeout(() => fetch(`/set?y=${e.target.value}`), 120);
  });

  el('colorpicker').addEventListener('input', (e) => {
    const hex = e.target.value;
    const r = parseInt(hex.substr(1,2), 16);
    const g = parseInt(hex.substr(3,2), 16);
    const b = parseInt(hex.substr(5,2), 16);
    fetch(`/set?r=${r}&g=${g}&b=${b}`);
  });

  async function loadStatus() {
    try {
      const res = await fetch('/status');
      const data = await res.json();
      el('brightness').value = data.brightness;
      el('brightness-value').textContent = data.brightness + '%';
      el('whitechan').value = data.w;
      el('white-value').textContent = data.w;
      el('yellowchan').value = data.y;
      el('yellow-value').textContent = data.y;
    } catch(e) { setStatusMsg('Nu s-a putut incarca starea'); }
  }
  loadStatus();
</script>
</body>
</html>
)HTML";

void handleRoot() {
  server.send(200, "text/html", PAGE_HTML);
}

// ----------- CITIRE COMENZI SERIALE (de la scriptul Python cu Whisper) -----------
// Scriptul trimite linii de forma "MOD_1\n" ... "MOD_5\n"
String serialBuffer = "";

void checkSerialCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        processSerialLine(serialBuffer);
      }
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }
}

void processSerialLine(const String& line) {
  Serial.print("Comanda seriala primita: ");
  Serial.println(line);

  if (line.startsWith("MOD_")) {
    int mod = line.substring(4).toInt();
    applyMode(mod);
  } else if (line == "OFF") {
    state.brightness = 0;
    applyOutput();
  } else if (line == "ON") {
    if (state.brightness == 0) state.brightness = 100;
    applyOutput();
  } else {
    Serial.println("Comanda seriala necunoscuta.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  ledcAttach(PIN_RED,    PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_GREEN,  PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_BLUE,   PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_WHITE,  PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_YELLOW, PWM_FREQ, PWM_RESOLUTION);
  applyOutput(); // pornim stins

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectare WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectat! IP-ul ESP32: ");
  Serial.println(WiFi.localIP());

  server.on("/",           handleRoot);
  server.on("/set",        handleSet);
  server.on("/mode",       handleMode);
  server.on("/brightness", handleBrightness);
  server.on("/off",        handleOff);
  server.on("/on",         handleOn);
  server.on("/status",     handleStatus);

  server.begin();
  Serial.println("Server HTTP pornit pe portul 80.");
  Serial.println("Astept si comenzi seriale: MOD_1..MOD_5, ON, OFF");
}

void loop() {
  server.handleClient();   // gestioneaza cererile din browser
  checkSerialCommands();   // gestioneaza comenzile vocale de la Python
}

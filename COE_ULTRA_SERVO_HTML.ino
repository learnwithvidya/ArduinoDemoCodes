#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

// ---------- Wi-Fi ----------
const char* WIFI_SSID = "AB_WIFI_73D3";
const char* WIFI_PASS = "12345678";

// ---------- Pins (GPIO numbers) ----------
#define LED_PIN    5   // D1
#define SERVO_PIN  4   // D2
#define TRIG_PIN  14   // D5
#define ECHO_PIN  12   // D6 (via divider to 3.3V)

// ---------- Logic ----------
const int ANGLE_OPEN  = 90;
const int ANGLE_CLOSE = 0;
const int THRESH_CM   = 25;

ESP8266WebServer server(80);
Servo gate;

bool ledOn = false;
bool autoMode = true;
int  servoAngle = ANGLE_OPEN;

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (!dur) return NAN;
  return dur / 58.3f;
}

String pageHTML() {
  String h;
  h.reserve(9000);

  h += "<!doctype html><html><head><meta charset='utf-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<title>ESP8266 Smart Gate</title>";
  h += "<style>";
  h += "body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:16px}";
  h += ".grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(280px,1fr))}";
  h += ".card{border:1px solid #ddd;border-radius:12px;padding:16px}";
  h += "button{padding:8px 12px;margin:4px 6px 0 0;cursor:pointer}";
  h += "label.switch{display:inline-flex;align-items:center;gap:8px;margin-top:8px}";
  h += "input[type=range]{width:100%}";
  h += "canvas{width:100%;height:160px;border:1px solid #eee;border-radius:8px}";
  h += ".muted{color:#666;font-size:12px}";
  h += "</style></head><body>";

  h += "<h2>ESP8266 Smart Gate — Local Dashboard</h2>";
  h += "<div class='grid'>";

  h += "<div class='card'><h3>Status</h3>";
  h += "<p>Distance: <b><span id='dist'>—</span> cm</b></p>";
  h += "<p>LED: <b><span id='led'>—</span></b> | Servo: <b><span id='ang'>—</span>°</b></p>";
  h += "<p>Mode: <b><span id='mode'>—</span></b></p>";
  h += "<canvas id='chart'></canvas>";
  h += "<p class='muted'>Chart shows last ~60 distance samples (auto-refresh).</p>";
  h += "</div>";

  h += "<div class='card'><h3>Controls</h3>";
  h += "<div><button onclick=\"act('/led?state=on')\">LED ON</button>";
  h += "<button onclick=\"act('/led?state=off')\">LED OFF</button></div>";
  h += "<div style='margin-top:8px'><button onclick=\"act('/open')\">Gate OPEN</button>";
  h += "<button onclick=\"act('/close')\">Gate CLOSE</button></div>";
  h += "<div style='margin-top:8px'>";
  h += "<label>Servo Angle: <span id='aval'>90</span>°</label>";
  h += "<input id='aslide' type='range' min='0' max='180' value='90'></div>";
  h += String("<label class='switch'><input id='auto' type='checkbox'> Auto Mode (") + THRESH_CM + " cm)</label>";
  h += String("<p class='muted'>Auto: closes if distance < ") + THRESH_CM + " cm, else opens.</p>";
  h += "</div>";

  h += "</div>"; // grid

  h += "<script>";
  h += "const distEl=document.getElementById('dist');";
  h += "const ledEl=document.getElementById('led');";
  h += "const angEl=document.getElementById('ang');";
  h += "const modeEl=document.getElementById('mode');";
  h += "const aslide=document.getElementById('aslide');";
  h += "const aval=document.getElementById('aval');";
  h += "const autock=document.getElementById('auto');";
  h += "const cvs=document.getElementById('chart');";
  h += "const ctx=cvs.getContext('2d');";
  h += "let data=[];";
  h += "function act(url){fetch(url).then(_=>refresh());}";
  h += "aslide.addEventListener('input',()=>{aval.textContent=aslide.value;});";
  h += "aslide.addEventListener('change',()=>{act('/servo?angle='+aslide.value)});";
  h += "autock.addEventListener('change',()=>{act('/auto?mode='+(autock.checked?'on':'off'));});";
  h += "function refresh(){fetch('/api/status').then(r=>r.json()).then(j=>{";
  h += "if(j.distance===null){distEl.textContent='—';}else{";
  h += "distEl.textContent=j.distance.toFixed(1);";
  h += "data.push(j.distance); if(data.length>60) data.shift(); draw();}";
  h += "ledEl.textContent=j.led?'ON':'OFF';";
  h += "angEl.textContent=j.angle;";
  h += "modeEl.textContent=j.auto?'AUTO':'MANUAL';";
  h += "autock.checked=!!j.auto;";
  h += "aslide.value=j.angle; aval.textContent=j.angle;";
  h += "}).catch(_=>{});}";

  h += "function draw(){";
  h += "const W=cvs.width=(cvs.clientWidth||600), H=cvs.height=160;";
  h += "ctx.clearRect(0,0,W,H); ctx.strokeStyle='#bbb'; ctx.strokeRect(0,0,W,H);";
  h += "if(data.length<2) return;";
  h += String("const min=Math.min(...data,0)-2, max=Math.max(...data,") + THRESH_CM + "+5)+2;";
  h += "const rng=(max-min)||1;";
  h += "ctx.beginPath(); ctx.lineWidth=2; ctx.strokeStyle='#0366d6';";
  h += "for(let i=0;i<data.length;i++){";
  h += "const x=i*(W/(data.length-1));";
  h += "const y=H-((data[i]-min)/rng)*H;";
  h += "if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);}";
  h += "ctx.stroke();";
  h += String("const thr=") + THRESH_CM + ";";
  h += "const ythr=H-((thr-min)/rng)*H;";
  h += "ctx.setLineDash([6,4]); ctx.strokeStyle='#d62828';";
  h += "ctx.beginPath(); ctx.moveTo(0,ythr); ctx.lineTo(W,ythr); ctx.stroke(); ctx.setLineDash([]);}";
  h += "refresh(); setInterval(refresh,2000);";
  h += "</script></body></html>";

  return h;
}

void setServoAngle(int a) {
  a = constrain(a, 0, 180);
  servoAngle = a;
  gate.write(a);
}

void setLed(bool on) {
  ledOn = on;
  // If you use an active-LOW relay, invert this:
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

// --------- HTTP handlers ----------
void handleRoot()             { server.send(200, "text/html", pageHTML()); }
void handleOpen()             { setServoAngle(ANGLE_OPEN);  server.send(200, "text/plain", "OK"); }
void handleClose()            { setServoAngle(ANGLE_CLOSE); server.send(200, "text/plain", "OK"); }
void handleServo()            { int a=server.hasArg("angle")?server.arg("angle").toInt():servoAngle; setServoAngle(a); server.send(200,"text/plain","OK"); }
void handleLed()              { String s=server.hasArg("state")?server.arg("state"):""; if(s=="on") setLed(true); if(s=="off") setLed(false); server.send(200,"text/plain","OK"); }
void handleAuto()             { String s=server.hasArg("mode")?server.arg("mode"):"on"; autoMode=(s=="on"); server.send(200,"text/plain","OK"); }
void handleStatus() {
  float d = readDistanceCm();
  if (autoMode && !isnan(d)) {
    if (d < THRESH_CM && servoAngle != ANGLE_CLOSE) setServoAngle(ANGLE_CLOSE);
    if (d >= THRESH_CM && servoAngle != ANGLE_OPEN)  setServoAngle(ANGLE_OPEN);
  }
  String out = "{\"distance\":";
  out += (isnan(d) ? String("null") : String(d,1));
  out += ",\"led\":";   out += (ledOn ? "true" : "false");
  out += ",\"angle\":"; out += servoAngle;
  out += ",\"auto\":";  out += (autoMode ? "true" : "false");
  out += "}";
  server.send(200, "application/json", out);
}

void setup() {
  pinMode(LED_PIN, OUTPUT); setLed(false);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  gate.attach(SERVO_PIN);
  setServoAngle(ANGLE_OPEN);

  Serial.begin(115200);
  Serial.println("\n[Session 3] ESP8266 Smart Gate Dashboard");
  Serial.print("Connecting to "); Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print("\nIP: "); Serial.println(WiFi.localIP());

  server.on("/",           HTTP_GET, handleRoot);
  server.on("/open",       HTTP_GET, handleOpen);
  server.on("/close",      HTTP_GET, handleClose);
  server.on("/servo",      HTTP_GET, handleServo);
  server.on("/led",        HTTP_GET, handleLed);
  server.on("/auto",       HTTP_GET, handleAuto);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.begin();
  Serial.println("HTTP server started on port 80.");
}

void loop() { server.handleClient(); }

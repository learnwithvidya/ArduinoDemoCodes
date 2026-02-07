/* 
  Session 4 — Safe Authentication Demo (ESP8266)
  - Hosts a simple login page (username/password)
  - Stores a weak credential for demo ("admin" / "admin123")
  - Logs attempts on-device (in RAM)
  - Instructor-only endpoint /simulate_attack simulates many attempts and
    demonstrates lockout and success in a safe, contained manner.
  - Use this to show the effect of weak passwords, and how lockout/rate-limit mitigates the issue.
  const char* WIFI_SSID = "AB_+********";
  const char* WIFI_PASS = "12345678";
*/

/*******************************************************
 * ESP8266 — Session 4: Login + Authenticated Dashboard
 * - Requires login before showing/using dashboard & APIs
 * - Uses a simple cookie "auth=1" (demo only)
 *******************************************************/


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

// ===== Wi-Fi (edit) =====
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASS = "password";

// ===== Demo credentials (edit for class) =====
const char* DEMO_USER = "admin";
const char* DEMO_PASS = "admin123";   // weak on purpose (demo)

// ===== Pins (GPIO numbers) =====
#define LED_PIN    5    // D1
#define SERVO_PIN  4    // D2
#define TRIG_PIN  14   // D5
#define ECHO_PIN  12   // D6 (through divider)

// ===== Logic =====
const int ANGLE_OPEN  = 90;
const int ANGLE_CLOSE = 0;
const int THRESH_CM   = 25;

ESP8266WebServer server(80);
Servo gate;

bool ledOn = false;
bool autoMode = true;
int  servoAngle = ANGLE_OPEN;

// ---- Utility: we must collect the Cookie header ----
const char* headerKeys[] = { "Cookie" };
const size_t headerKeyCount = 1;

// ---- Helpers ----
bool isAuthenticated() {
  if (!server.hasHeader("Cookie")) return false;
  String c = server.header("Cookie");
  return (c.indexOf("auth=1") >= 0);
}

void sendRedirect(const String& to) {
  server.sendHeader("Location", to);
  // 303 is “See Other” which works well after POST
  server.send(303, "text/plain", "");
}

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (!dur) return NAN;
  return dur / 58.3f;
}

// ---- HTML ----
String loginPage(const String& msg = "") {
  String h;
  h += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<title>Device Login (Demo)</title><style>body{font-family:Arial;margin:20px}input{padding:6px;margin:4px}button{padding:8px 12px}</style></head><body>";
  h += "<h2>Device Login (Demo)</h2>";
  if (msg.length()) h += "<p style='color:red'>" + msg + "</p>";
  h += "<form method='POST' action='/do_login'>"
       "Username:<br><input name='user' autocomplete='username'><br>"
       "Password:<br><input name='pass' type='password' autocomplete='current-password'><br><br>"
       "<button type='submit'>Login</button></form>";
  h += "<p><small>Training demo on local network. No HTTPS or strong sessions here.</small></p>";
  h += "</body></html>";
  return h;
}

String dashboardHTML() {
  String h;
  h += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<title>ESP Dashboard</title><style>body{font-family:Arial;margin:16px}.card{border:1px solid #ddd;padding:12px;border-radius:8px}button{padding:8px 10px;margin:4px}</style></head><body>";
  h += "<h2>ESP Device Dashboard</h2><p><a href='/logout'>Logout</a></p>";
  h += "<div class='card'><h3>Status</h3>"
       "Distance: <b><span id='dist'>—</span> cm</b><br>"
       "LED: <b><span id='led'>—</span></b> | Servo: <b><span id='ang'>—</span>°</b><br>"
       "Mode: <b><span id='mode'>—</span></b></div>";
  h += "<div class='card' style='margin-top:8px'><h3>Controls</h3>";
  h += "<button onclick=\"fetch('/led?state=on').then(_=>refresh())\">LED ON</button>";
  h += "<button onclick=\"fetch('/led?state=off').then(_=>refresh())\">LED OFF</button>";
  h += "<br><button onclick=\"fetch('/open').then(_=>refresh())\">Gate OPEN</button>";
  h += "<button onclick=\"fetch('/close').then(_=>refresh())\">Gate CLOSE</button>";
  h += "<p>Servo Angle: <input id='angle' type='range' min='0' max='180' value='90' oninput='angv.value=value' onchange=\"fetch('/servo?angle='+this.value).then(_=>refresh())\"><output id='angv'>90</output></p>";
  h += "<p><label><input id='auto' type='checkbox' onchange=\"fetch('/auto?mode='+(this.checked?'on':'off')).then(_=>refresh())\"> Auto Mode (" + String(THRESH_CM) + " cm)</label></p></div>";
  h += "<script>"
       "function refresh(){fetch('/api/status').then(r=>{if(!r.ok){location.href='/login';return;}return r.json()}).then(j=>{if(!j)return;"
       "document.getElementById('dist').textContent = (j.distance===null)?'—':j.distance.toFixed(1);"
       "document.getElementById('led').textContent = j.led?'ON':'OFF';"
       "document.getElementById('ang').textContent = j.angle;"
       "document.getElementById('mode').textContent = j.auto?'AUTO':'MANUAL';"
       "document.getElementById('angle').value = j.angle; document.getElementById('angv').value=j.angle;});}"
       "refresh(); setInterval(refresh,2000);"
       "</script></body></html>";
  return h;
}

// ---- Handlers ----
void handleLoginGet() {
  if (isAuthenticated()) { sendRedirect("/"); return; }
  server.send(200, "text/html", loginPage());
}

void handleDoLogin() {
  String user = server.arg("user");
  String pass = server.arg("pass");

  if (user == DEMO_USER && pass == DEMO_PASS) {
    server.sendHeader("Set-Cookie", "auth=1; Path=/; HttpOnly");
    server.sendHeader("Cache-Control", "no-store");
    // more robust than bare redirect: HTML + JS redirect
    String ok = "<!doctype html><meta charset='utf-8'><title>Login OK</title>"
                "<p>Login successful. Redirecting…</p>"
                "<script>location.href='/'</script>"
                "<p><a href='/'>Continue</a></p>";
    server.send(200, "text/html", ok);
  } else {
    server.send(200, "text/html", loginPage("Invalid credentials. Try again."));
  }
}

void handleLogout() {
  server.sendHeader("Set-Cookie", "auth=0; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
  sendRedirect("/login");
}

void handleRoot() {
  if (!isAuthenticated()) { sendRedirect("/login"); return; }
  server.send(200, "text/html", dashboardHTML());
}

void handleStatus() {
  if (!isAuthenticated()) { server.send(401, "text/plain", "Unauthorized"); return; }

  float d = readDistanceCm();
  if (autoMode && !isnan(d)) {
    if (d < THRESH_CM && servoAngle != ANGLE_CLOSE) { servoAngle = ANGLE_CLOSE; gate.write(servoAngle); }
    if (d >= THRESH_CM && servoAngle != ANGLE_OPEN)  { servoAngle = ANGLE_OPEN;  gate.write(servoAngle); }
  }

  String out = "{\"distance\":";
  out += (isnan(d) ? String("null") : String(d,1));
  out += ",\"led\":";   out += (ledOn ? "true" : "false");
  out += ",\"angle\":"; out += String(servoAngle);
  out += ",\"auto\":";  out += (autoMode ? "true" : "false");
  out += "}";
  server.send(200, "application/json", out);
}

void handleLed() {
  if (!isAuthenticated()) { server.send(401, "text/plain", "Unauthorized"); return; }
  String s = server.hasArg("state") ? server.arg("state") : "";
  if (s == "on")  { ledOn = true;  digitalWrite(LED_PIN, HIGH); }
  if (s == "off") { ledOn = false; digitalWrite(LED_PIN, LOW);  }
  server.send(200, "text/plain", "OK");
}

void handleOpen()  { if (!isAuthenticated()) { server.send(401, "text/plain", "Unauthorized"); return; } servoAngle = ANGLE_OPEN;  gate.write(servoAngle); server.send(200,"text/plain","OK"); }
void handleClose() { if (!isAuthenticated()) { server.send(401, "text/plain", "Unauthorized"); return; } servoAngle = ANGLE_CLOSE; gate.write(servoAngle); server.send(200,"text/plain","OK"); }

void handleServo() {
  if (!isAuthenticated()) { server.send(401, "text/plain", "Unauthorized"); return; }
  int a = server.hasArg("angle") ? server.arg("angle").toInt() : servoAngle;
  a = constrain(a, 0, 180);
  servoAngle = a; gate.write(servoAngle);
  server.send(200, "text/plain", "OK");
}

void handleAuto() {
  if (!isAuthenticated()) { server.send(401, "text/plain", "Unauthorized"); return; }
  String s = server.hasArg("mode") ? server.arg("mode") : "on";
  autoMode = (s == "on");
  server.send(200, "text/plain", "OK");
}

// Diagnostics: useful while testing cookies
void handleWhoAmI() {
  String info = "Has Cookie header: ";
  info += server.hasHeader("Cookie") ? "yes\n" : "no\n";
  if (server.hasHeader("Cookie")) {
    info += "Cookie: "; info += server.header("Cookie"); info += "\n";
  }
  info += String("Authenticated: ") + (isAuthenticated() ? "yes" : "no") + "\n";
  server.send(200, "text/plain", info);
}

// ---- setup/loop ----
void setup() {
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  gate.attach(SERVO_PIN);
  servoAngle = ANGLE_OPEN; gate.write(servoAngle);

  Serial.begin(115200);
  Serial.println();
  Serial.print("Connecting to "); Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println(); Serial.print("IP: "); Serial.println(WiFi.localIP());

  // IMPORTANT: collect headers we want to read (Cookie)
  server.collectHeaders(headerKeys, headerKeyCount);

  // Routes
  server.on("/login",     HTTP_GET,  handleLoginGet);
  server.on("/do_login",  HTTP_POST, handleDoLogin);
  server.on("/logout",    HTTP_GET,  handleLogout);

  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/api/status", HTTP_GET,  handleStatus);
  server.on("/led",        HTTP_GET,  handleLed);
  server.on("/open",       HTTP_GET,  handleOpen);
  server.on("/close",      HTTP_GET,  handleClose);
  server.on("/servo",      HTTP_GET,  handleServo);
  server.on("/auto",       HTTP_GET,  handleAuto);

  server.on("/whoami",     HTTP_GET,  handleWhoAmI); // diagnostic

  server.begin();
  Serial.println("HTTP server started");
}

void loop() { server.handleClient(); }

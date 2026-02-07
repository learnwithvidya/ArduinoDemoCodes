/*******************************************************
 * Session 2: Serial-Controlled Smart Gate (ESP8266)
 * - Ultrasonic distance (HC-SR04)
 * - Servo gate control (SG90)
 * - LED on/off
 * - Commands via Serial Monitor (9600 baud):
 *   open | close | led on | led off | angle <0..180> | help
 *******************************************************/
#include <Arduino.h>
#include <Servo.h>

// -------- Pin mapping (NodeMCU GPIO numbers) --------
// DO NOT use D1/D2... here
#define LED_PIN     5    // D1 pin on NodeMCU
#define SERVO_PIN   4    // D2 pin on NodeMCU
#define TRIG_PIN   14    // D5 pin on NodeMCU
#define ECHO_PIN   12    // D6 pin on NodeMCU (through divider!)

// -------- Servo angles --------
const int ANGLE_OPEN  = 90;
const int ANGLE_CLOSE = 0;

// -------- Timing --------
unsigned long lastReadMs = 0;
const unsigned long READ_INTERVAL_MS = 500;

// -------- Globals --------
Servo gate;
String cmd;

// -------- Helpers --------
float readDistanceCm() {
  // Trigger pulse
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Echo pulse width (timeout ~30ms)
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return NAN;  // timeout or out of range

  // Convert to cm (sound speed ~343 m/s)
  float distance = duration / 58.3; // cm
  return distance;
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  open            -> Servo to 90 deg"));
  Serial.println(F("  close           -> Servo to 0 deg"));
  Serial.println(F("  led on          -> LED ON"));
  Serial.println(F("  led off         -> LED OFF"));
  Serial.println(F("  angle <0..180>  -> Servo to angle"));
  Serial.println(F("  help            -> Show commands"));
}

void setServoAngle(int angle) {
  angle = constrain(angle, 0, 180);
  gate.write(angle);
  Serial.print(F("Servo angle -> ")); Serial.println(angle);
}

void handleCommand(String line) {
  line.trim();
  line.toLowerCase();

  if (line == "open") {
    setServoAngle(ANGLE_OPEN);
  } else if (line == "close") {
    setServoAngle(ANGLE_CLOSE);
  } else if (line == "led on") {
    digitalWrite(LED_PIN, HIGH);
    Serial.println(F("LED -> ON"));
  } else if (line == "led off") {
    digitalWrite(LED_PIN, LOW);
    Serial.println(F("LED -> OFF"));
  } else if (line.startsWith("angle ")) {
    int sp = line.indexOf(' ');
    int val = line.substring(sp + 1).toInt();
    setServoAngle(val);
  } else if (line == "help" || line == "?") {
    printHelp();
  } else if (line.length() > 0) {
    Serial.print(F("Unknown command: "));
    Serial.println(line);
    printHelp();
  }
}

void setup() {
  // I/O
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Serial
  Serial.begin(9600);
  delay(300);
  Serial.println(F("\n== Serial-Controlled Smart Gate (ESP8266) =="));
  Serial.println(F("Type 'help' for commands."));

  // Servo
  gate.attach(SERVO_PIN);
  setServoAngle(ANGLE_CLOSE); // start closed
}

void loop() {
  // --- Periodic distance read/print ---
  unsigned long now = millis();
  if (now - lastReadMs >= READ_INTERVAL_MS) {
    lastReadMs = now;
    float d = readDistanceCm();
    Serial.print(F("Distance: "));
    if (isnan(d)) Serial.println(F("— (out of range)"));
    else          Serial.print(d, 1), Serial.println(F(" cm"));
  }

  // --- Collect and process serial commands ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;    // ignore CR
    if (c == '\n') {            // end of line -> process
      handleCommand(cmd);
      cmd = "";
    } else {
      cmd += c;
      if (cmd.length() > 60) cmd = ""; // basic overflow guard
    }
  }
}

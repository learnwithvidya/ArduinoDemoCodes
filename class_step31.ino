/*
 * Ultrasonic Sensor with Servo and LED (Tinkercad)
 * Wiring assumed from the diagram:
 *  - HC-SR04: Trig -> D7, Echo -> D6, Vcc -> 5V, GND -> GND
 *  - Servo:  Signal -> D9, Vcc -> 5V, GND -> GND
 *  - LED:    Anode -> D13 (through resistor), Cathode -> GND
 */

#include <Servo.h>

const int trigPin = 9;
const int echoPin = 10;
const int ledPin  = 12;
const int servoPin = 3;

const int thresholdDistance = 40;   // cm (open if object is nearer than this)
const int openAngle  = 90;          // gate open angle
const int closeAngle = 0;           // gate closed angle

Servo gateServo;

long readDistanceCm() {
  // Trigger a 10 µs pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echo with a timeout (30 ms ≈ ~5 m max). Returns 0 if no echo.
  unsigned long duration = pulseIn(echoPin, HIGH, 30000UL);

  if (duration == 0) return 9999;            // treat as "out of range"
  return (long)(duration * 0.0343 / 2.0);    // distance in cm
}

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);

  gateServo.attach(servoPin);
  gateServo.write(closeAngle);

  Serial.begin(9600);
}

void loop() {
  long distance = readDistanceCm();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance <= thresholdDistance) {
    digitalWrite(ledPin, HIGH);
    gateServo.write(openAngle);
    delay(1500);                 // hold open
    gateServo.write(closeAngle);
  } else {
    digitalWrite(ledPin, LOW);
    gateServo.write(closeAngle);
  }

  delay(200);
}

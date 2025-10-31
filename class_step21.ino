/*
 * Ultrasonic Sensor with LED Blink Program
 * This program uses an HC-SR04 ultrasonic sensor to detect nearby objects.
 * If an object is within the specified distance, an LED blinks.
 */

// Define pin numbers for the ultrasonic sensor
const int trigPin = 9; // Trigger pin sends an ultrasonic pulse
const int echoPin = 10; // Echo pin receives the reflected pulse

// Define pin number for the LED
const int ledPin = 12; // LED is connected to digital pin 12

// Variable to store the calculated distance
long distance;

// Define the maximum distance to trigger the LED (in centimeters)
const int thresholdDistance = 20; // LED blinks if an object is closer than 20 cm

void setup() {
  // Initialize the ultrasonic sensor pins
  pinMode(trigPin, OUTPUT); // Trig pin set as output
  pinMode(echoPin, INPUT);  // Echo pin set as input
  
  // Initialize the LED pin
  pinMode(ledPin, OUTPUT);
  
  // Start the serial communication for debugging (optional)
  Serial.begin(9600); // Set baud rate for serial communication
}

void loop() {
  // Send an ultrasonic pulse
  digitalWrite(trigPin, LOW); // Ensure the trigger pin is low
  delayMicroseconds(2);       // Wait for 2 microseconds
  digitalWrite(trigPin, HIGH); // Send a 10-microsecond pulse
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure the duration of the echo pulse
  long duration = pulseIn(echoPin, HIGH);

  // Calculate distance in centimeters
  distance = duration * 0.034 / 2; // Speed of sound: 0.034 cm/µs, divide by 2 for round trip
  
  // Print the distance for debugging
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Check if the object is within the threshold distance
  if (distance <= thresholdDistance) {
    // Blink the LED
    digitalWrite(ledPin, HIGH); // Turn LED ON
    delay(500);                 // Keep LED ON for 500 milliseconds
    digitalWrite(ledPin, LOW);  // Turn LED OFF
    delay(500);                 // Keep LED OFF for 500 milliseconds
  } else {
    // Ensure the LED is OFF when no object is nearby
    digitalWrite(ledPin, LOW);
  }
}


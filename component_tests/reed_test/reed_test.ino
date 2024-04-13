#include <Arduino.h>

const int reedPin = 23; // GPIO pin connected to the reed switch

void setup() {
  Serial.begin(115200);          // Start the Serial communication
  pinMode(reedPin, INPUT_PULLUP); // Set the reed switch pin as input with internal pull-up resistor
}

void loop() {
  int reedState = digitalRead(reedPin); // Read the state of the reed switch
  if (reedState == LOW) { // If the reed switch is closed (magnet detected)
    Serial.println("Magnet Detected!");
  } 
  else {
    Serial.println("No Magnet Detected.");
  }
  Serial.println("Still Checking");
  delay(500); // Delay to prevent spamming (adjust as necessary)
}


const int switchPin = 2;  // Change to your actual GPIO pin number

void setup() {
  Serial.begin(115200);
  pinMode(switchPin, INPUT_PULLUP);
}

void loop() {
  int switchState = digitalRead(switchPin);

  if (switchState == LOW) {
    Serial.println("Mailbox Opened");
    // Add code here to send data to your IoT platform or service
  }

  delay(1000);  // Adjust delay as needed
}

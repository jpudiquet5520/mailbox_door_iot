#include <WiFi.h> // Include the WiFi library

const char* ssid     = "Lodge of La Mesa";     // Replace with your WiFi network name
const char* password = "welcome!"; // Replace with your WiFi password

void setup() {
  Serial.begin(115200);        // Start the Serial communication to output the status
  WiFi.begin(ssid, password);  // Begin connecting to WiFi

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { // Wait until the connection has been established
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // Print the local IP address
}

void loop() {
  // Nothing to do here
}
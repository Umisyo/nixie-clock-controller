#include <Arduino.h>
#include <WiFi.h>

// put function declarations here:
int myFunction(int, int);


// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}

void connectToWiFi() {
  WiFi.begin("SSID", "PASSWORD");
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(12, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Hello, World!");
  digitalWrite(12, HIGH);
  delay(1000);
  digitalWrite(12, LOW);
  delay(1000);
}

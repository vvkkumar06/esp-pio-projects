#include <ESPmDNS.h>
#include "motor-controls.h"
#include "camera-config.h"
#include "server.h"

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Motor pin modes
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  pinMode(headLight, OUTPUT);
  // pinMode(rightIndicator, OUTPUT);
  // pinMode(leftIndicator, OUTPUT);
  pinMode(breakLight, OUTPUT);

  stopCar();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());
  if (!MDNS.begin("esp32scar"))
  {
    Serial.println("Error starting mDNS");
    return;
  }
  Serial.println("mDNS responder started");

  delay(3000);
  if (!startCamera())
  {
    Serial.println("Camera init failed. Halting.");
    while (true)
      delay(1000);
  }
  Serial.println("Connecting to Firebase...");
  routes();
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.println("HTTP stream server started");
  // Serial.end();
}

void loop()
{
  server.handleClient();
  delay(10);
}

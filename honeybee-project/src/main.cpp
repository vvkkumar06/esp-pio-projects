#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include "DHT.h"
#include "secrets.h"
#include "utils.h"

// DHT11 setup
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Oracle APEX server details
const char *host = APEX_SERVER_HOST;
const int httpsPort = APEX_SERVER_PORT;
const char *path = APEX_SERVER_PATH;

void setup()
{
  Serial.begin(115200);
  dht.begin();

  // Connect to WiFi
  connectWiFi(WIFI_SSID, WIFI_PASSWORD);
}
void loop()
{
  // Read DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t))
  {
    Serial.println("Failed to read DHT11");
    delay(5000);
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClientSecure client;
    client.setInsecure(); // bypass SSL verification

    HttpClient http(client, host, httpsPort);
    http.beginRequest();
    http.post(path);
    http.sendHeader("Content-Type", "application/json");

    // Build JSON payload
    String jsonPayload = buildPayload(BEEHIVE_ID, t, h, 6.2);

    http.sendHeader("Content-Length", jsonPayload.length());
    http.beginBody();
    http.print(jsonPayload);
    http.endRequest();

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    Serial.println("Status code: " + String(statusCode));
    Serial.println("Response: " + response);
  }
  else
  {
    Serial.println("WiFi disconnected");
  }

  delay(5000);
}

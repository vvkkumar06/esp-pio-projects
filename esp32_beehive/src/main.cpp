#include <WiFi.h>
#include "BeehiveMonitor.h"

// WiFi credentials
const char* WIFI_SSID = "K-1003";
const char* WIFI_PASSWORD = "Ashajha_123";

BeehiveMonitor beehive(2, DHT11);

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

    // Set APEX server details
    beehive.setApexServer("oracleapex.com", 443, "/ords/beehive/api/data");

    beehive.begin();
}

void loop() {
    beehive.sendData(1, 6.2);
    delay(5000);
}

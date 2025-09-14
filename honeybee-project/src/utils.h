#ifndef UTILS_H
#define UTILS_H

#include <WiFi.h>
#include <time.h>
#include "DHT.h"

// Connect to WiFi
void connectWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
}

// Initialize NTP for current UTC time
void initTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // UTC
    Serial.print("Waiting for NTP time");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {  // Wait until time is set
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("\nTime initialized");
}

// Get current UTC time as ISO 8601 string
String getCurrentTime() {
    time_t now = time(nullptr);
    struct tm* tm_info = gmtime(&now);  // UTC
    char buffer[25];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return String(buffer);
}

// Build JSON payload from DHT11 readings and optional weight
String buildPayload(int beehiveId, float temperature, float humidity, float weight) {
    String json = "{";
    json += "\"BEEHIVE_ID\":" + String(beehiveId) +",";
    json += "\"TEMPERATURE\":" + String(temperature, 1) + ",";
    json += "\"HUMIDITY\":" + String(humidity, 1) + ",";
    json += "\"WEIGHT\":" + String(weight, 2) + ",";
    json += "\"CREATED_DATE\":\"" + getCurrentTime() + "\"";
    json += "}";
    return json;
}

#endif

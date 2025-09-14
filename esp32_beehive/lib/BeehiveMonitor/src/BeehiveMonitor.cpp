#include "BeehiveMonitor.h"
#include <WiFi.h>

BeehiveMonitor::BeehiveMonitor(int dhtPin, int dhtType) : dht(dhtPin, dhtType) {}

void BeehiveMonitor::setApexServer(const char* host, int port, const char* path) {
    apexHost = host;
    apexPort = port;
    apexPath = path;
}

void BeehiveMonitor::begin() {
    dht.begin();
    initTime();
    // Whitelist check (fully internal)
    if (!isWhitelisted()) {
        Serial.println("Device not authorized! Contact admin to whitelist.");
        while (true) delay(1000);  // Block forever
    }
}

void BeehiveMonitor::sendData(int beehiveId, float weight) {
    WiFiClientSecure client;
    client.setInsecure();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    String payload = buildPayload(beehiveId, temperature, humidity, weight);

    HttpClient http(client, apexHost, apexPort);
    http.beginRequest();
    http.post(apexPath);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", payload.length());
    http.beginBody();
    http.print(payload);
    http.endRequest();

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    Serial.print("Data send status: "); Serial.println(statusCode);
    Serial.print("Response: "); Serial.println(response);
}

bool BeehiveMonitor::isWhitelisted() {
    WiFiClientSecure client;
    client.setInsecure();
    String mac = WiFi.macAddress();
    HttpClient http(client, whitelistHost, whitelistPort);

    // Build URL with MAC as query parameter
    String pathWithMac = String(whitelistPath) + "?mac=" + mac;

    Serial.println("Checking whitelist for MAC: " + mac);

    int err = http.get(pathWithMac);
    if (err < 0) {
        Serial.print("HTTP request failed, error code: ");
        Serial.println(err);
        if (!client.connected()) Serial.println("Client not connected!");
        Serial.println("Warning: Cannot reach whitelist API, allowing device to operate.");
        return true; // Fail-safe
    }

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    Serial.print("HTTP Status Code: "); Serial.println(statusCode);

    if (statusCode != 200) {
        Serial.println("Failed to check whitelist! Status code not 200.");
        Serial.println("Warning: Allowing device to operate due to API failure.");
        return true; // Fail-safe
    }

    if (response.indexOf("true") != -1) { // Netlify API returns {"authorized":true}
        Serial.println("Device authorized ✅");
        return true;
    }

    Serial.println("Device not found in whitelist ❌");
    return false;
}

String BeehiveMonitor::buildPayload(int beehiveId, float temperature, float humidity, float weight) {
    String json = "{";
    json += "\"BEEHIVE_ID\":" + String(beehiveId) +",";
    json += "\"TEMPERATURE\":" + String(temperature, 1) + ",";
    json += "\"HUMIDITY\":" + String(humidity, 1) + ",";
    json += "\"WEIGHT\":" + String(weight, 2) + ",";
    json += "\"CREATED_DATE\":\"" + getCurrentTime() + "\"";
    json += "}";
    return json;
}

void BeehiveMonitor::initTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    delay(2000);
}

String BeehiveMonitor::getCurrentTime() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "1970-01-01T00:00:00Z";
    }
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buf);
}

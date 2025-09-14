#ifndef BEEHIVEMONITOR_H
#define BEEHIVEMONITOR_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include "DHT.h"

class BeehiveMonitor {
  public:
    BeehiveMonitor(int dhtPin, int dhtType);

    // Initialize DHT sensor, time, and whitelist check
    void begin();

    // Set Oracle APEX server details once
    void setApexServer(const char* host, int port, const char* path);

    // Send sensor data to APEX
    void sendData(int beehiveId, float weight);

  private:
    DHT dht;
    bool authorized = false; 
    // Oracle APEX server (set by client)
    const char* apexHost = nullptr;
    int apexPort = 0;
    const char* apexPath = nullptr;

    // Private whitelist (hidden from client)
    const char* whitelistHost = "beehive-mac-check.netlify.app";
    const int whitelistPort = 443;
    const char* whitelistPath = "/.netlify/functions/checkWhitelist";

    void initTime();
    String getCurrentTime();
    String buildPayload(int beehiveId, float temperature, float humidity, float weight);
    bool isWhitelisted();
};

#endif // BEEHIVEMONITOR_H

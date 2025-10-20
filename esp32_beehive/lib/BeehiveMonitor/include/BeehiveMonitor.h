#ifndef BEEHIVEMONITOR_H
#define BEEHIVEMONITOR_H
#define TINY_GSM_MODEM_SIM7600
#include <Arduino.h>
#include <TinyGsmClient.h>
#include <SSLClient.h>
#include <ArduinoHttpClient.h>
#include "DHT.h"
#include "HX711.h"
#include "time.h"
#include "sys/time.h"
#include "nvs_flash.h"
#include "BeehivePreferences.h"

#define SerialAT Serial1
#define SerialMon Serial
class BeehiveMonitor
{
public:
  BeehiveMonitor(int dhtPin, int dhtType, int hxDataPin, int hxSckPin, int RXPin, int TXPin);

  // Initialize DHT sensor, time, and whitelist check
  void begin();

  // Set Oracle APEX server details once
  void setApexServer(const char *host, int port, const char *path);

  // Send sensor data to APEX
  void sendData(int beehiveId);

  void calibrateScale(float knownWeightKg);

  void initializeModem();

private:
  DHT dht;
  HX711 scale;
  // Oracle APEX server (set by client)
  const char *apexHost = nullptr;
  int apexPort = 0;
  const char *apexPath = nullptr;

  // Private whitelist (hidden from client)
  const char *whitelistHost = "beehive-mac-check.netlify.app";
  const int whitelistPort = 443;
  const char *whitelistPath = "/.netlify/functions/checkWhitelist";

  void initTime();
  String getCurrentTime();
  String buildPayload(int beehiveId, float temperature, float humidity, float weight);
  bool isWhitelisted();
  float getWeight();
  TinyGsm modem;
  TinyGsmClient baseClient;
  const long gmtOffset_sec = 19800; // +5:30 IST in seconds
  const int daylightOffset_sec = 0; // No daylight saving in India
  float loadCalibration();
  void saveCalibration(float factor);
  static void initNVS();
  const int calibPin = 4; // Pin to trigger calibration mode
  const int CALIB_LED = 21; // Onboard LED to indicate calibration mode
  const int ONLINE_LED = 22;
  const int OFFLINE_LED = 23;
  BeehivePreferences prefs;
};

#endif // BEEHIVEMONITOR_H

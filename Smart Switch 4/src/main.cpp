#define ENABLE_SERVICE_AUTH
#define ENABLE_DATABASE
#include "secrets.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <ArduinoJson.h>
#include <FirebaseJson.h>
#include "ExampleFunctions.h"
#include <WiFiManager.h>

const long gmtOffset_sec = 19800; // +5:30 IST
const int daylightOffset_sec = 0;
const char *ntpServer = "pool.ntp.org";

// --- Relay handling ---
struct SwitchRelay {
  String path;
  bool currentState;
  int pin;
};
#define NUM_RELAYS 4   // Change depending on number of relays

SwitchRelay switchRelays[NUM_RELAYS] = {
  {"/switch_1", false, D1},
  {"/switch_2", false, D2},
  {"/switch_3", false, D5},
  {"/switch_4", false, D6}
};

// --- Firebase setup ---
ServiceAuth sa_auth(FIREBASE_CLIENT_EMAIL, FIREBASE_PROJECT_ID, PRIVATE_KEY, 3000);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase rtdb;
AsyncResult dbResult;

#define WIFI_RESET_PIN D4
#define OFFLINE_MODE   D7
#define ONLINE_MODE    D8
FirebaseJsonData result;

WiFiManager wm;
bool isOnline = false;

// --- Functions ---
void setupRelayPins();
void changeRelayState(const String &path, bool state);
bool getTimeFromNTP(uint8_t maxRetries = 10);
void syncStatesToFirebase();
uint32_t get_ntp_server_time();
void setFirebaseTimeSafelyWithRetry(uint8_t maxRetries = 5);

void indicateOffline() {
  digitalWrite(OFFLINE_MODE, HIGH);
  digitalWrite(ONLINE_MODE, LOW);
  isOnline = false;
}
void indicateOnline() {
  digitalWrite(OFFLINE_MODE, LOW);
  digitalWrite(ONLINE_MODE, HIGH);
  syncStatesToFirebase();
  isOnline = true;
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  pinMode(OFFLINE_MODE, OUTPUT);
  pinMode(ONLINE_MODE, OUTPUT);
  indicateOffline();

  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    Serial.println("WiFi reset button held down — clearing credentials...");
    wm.resetSettings();
    delay(1000);
    ESP.restart();
  }

  setupRelayPins();

  if (!wm.autoConnect("SmartSwitch-Setup", "admin@123")) {
    Serial.println("Failed to connect, restarting...");
    delay(3000);
    ESP.restart();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    if (getTimeFromNTP()) {
      Serial.println("Connecting to Firebase...");
      set_ssl_client_insecure_and_buffer(ssl_client);
      setFirebaseTimeSafelyWithRetry();
      initializeApp(aClient, app, getAuth(sa_auth), auth_debug_print, "🔐 authTask");
      app.getApp<RealtimeDatabase>(rtdb);
      rtdb.url(FIREBASE_REALTIME_DATABASE_URL);
      indicateOnline();
    } else {
      Serial.println("NTP failed, switching offline");
      indicateOffline();
    }
  } else {
    Serial.println("WiFi failed, switching offline");
    indicateOffline();
  }
}

// --- Loop ---
void loop() {
  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    Serial.println("WiFi reset button held down — clearing credentials...");
    wm.resetSettings();
    delay(1000);
    ESP.restart();
  }

  app.loop();

  if (app.ready()) {
    FirebaseJson json = rtdb.get<String>(aClient, SWITCHBOARD_PATH);
    if (aClient.lastError().code() == 0) {
      if (!isOnline) {
        indicateOnline();
        json = rtdb.get<String>(aClient, SWITCHBOARD_PATH);
      }
      size_t len = json.iteratorBegin();
      String key, value;
      int type;
      for (size_t i = 0; i < len; i++) {
        json.iteratorGet(i, type, key, value);
        if (json.get(result, key)) {
          for (int r = 0; r < NUM_RELAYS; r++) {
            if (switchRelays[r].path == "/" + key) {
              changeRelayState(switchRelays[r].path, result.to<bool>());
            }
          }
        } else {
          indicateOffline();
          Serial.printf("%s: key not found\n", key.c_str());
        }
      }
      json.iteratorEnd();
    } else {
      indicateOffline();
      Firebase.printf("Error: %s (code %d)\n", aClient.lastError().message().c_str(), aClient.lastError().code());
    }
  } else {
    indicateOffline();
  }
}

// --- Relay Setup ---
void setupRelayPins() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(switchRelays[i].pin, OUTPUT);
    // Set all OFF initially
    digitalWrite(switchRelays[i].pin, RELAY_ACTIVE_HIGH ? LOW : HIGH);
  }
}

// --- Relay Change ---
void changeRelayState(const String &path, bool newState) {
  const String prefix = "/switch_";
  if (!path.startsWith(prefix)) {
    Serial.printf("Invalid path: %s\n", path.c_str());
    return;
  }
  int relayIndex = path.substring(prefix.length()).toInt() - 1;
  if (relayIndex < 0 || relayIndex >= NUM_RELAYS) {
    Serial.printf("Invalid relay index from path: %s\n", path.c_str());
    return;
  }
  if (switchRelays[relayIndex].currentState != newState) {
    // Handle active HIGH or active LOW
    if (newState) {
      digitalWrite(switchRelays[relayIndex].pin, RELAY_ACTIVE_HIGH ? HIGH : LOW); 
    } else {
      digitalWrite(switchRelays[relayIndex].pin, RELAY_ACTIVE_HIGH ? LOW : HIGH);
    }
    switchRelays[relayIndex].currentState = newState;
    Serial.printf("Switch_%d : %s\n", relayIndex + 1, newState ? "ON" : "OFF");
  }
}

// --- Time Sync ---
bool getTimeFromNTP(uint8_t maxRetries) {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  for (int attempts = 0; attempts < maxRetries; attempts++) {
    if (getLocalTime(&timeinfo)) {
      Serial.println("✅ NTP time acquired");
      return true;
    }
    Serial.printf("❌ NTP attempt %d failed\n", attempts + 1);
    delay(1000);
  }
  Serial.println("🚫 NTP failed");
  return false;
}

// --- Firebase Sync ---
void syncStatesToFirebase() {
  if (!isOnline) {
    for (int i = 0; i < NUM_RELAYS; i++) {
      digitalWrite(ONLINE_MODE, LOW);
      String fullPath = SWITCHBOARD_PATH + String("/") + switchRelays[i].path;
      if (rtdb.set(aClient, fullPath, switchRelays[i].currentState)) {
        digitalWrite(ONLINE_MODE, HIGH);
        Serial.printf("Synced %s: %s\n", switchRelays[i].path.c_str(), switchRelays[i].currentState ? "ON" : "OFF");
      } else {
        digitalWrite(ONLINE_MODE, HIGH);
        Serial.printf("Failed to sync %s: %s\n", switchRelays[i].path.c_str(), aClient.lastError().message().c_str());
      }
      delay(100);
    }
  }
}

// --- NTP helper ---
uint32_t get_ntp_server_time() {
  uint32_t ts = 0;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return 0;
  }
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  int retry = 0;
  while (time(nullptr) < 1000000000 && retry < 20) {
    delay(500);
    retry++;
  }
  ts = time(nullptr);
  return ts;
}

void setFirebaseTimeSafelyWithRetry(uint8_t maxRetries) {
  if (WiFi.status() != WL_CONNECTED) return;
  uint8_t attempt = 0;
  while (attempt < maxRetries) {
    uint32_t ts = get_ntp_server_time();
    if (ts > 0) {
      app.setTime(ts);
      Serial.printf("✅ Firebase time set: %lu\n", ts);
      return;
    }
    Serial.println("❌ Failed to set NTP time, retrying...");
    delay(2000);
    attempt++;
  }
  Serial.println("🚫 Giving up on NTP");
}
